// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <random>
#include <string_view>

#include "base/base_paths.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/atspi_in_process_fuzzer.pb.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "sql/database.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/base/glib/scoped_gobject.h"

// Controls (by name) which we shouldn't choose.
constexpr auto kBlockedControls =
    base::MakeFixedFlatSet<std::string_view>({"Close"});

using ScopedAtspiAccessible = ScopedGObject<AtspiAccessible>;

// We inform centipede of control paths we've explored, to
// bias centipede towards exploring new controls.
static constexpr size_t kNumControlsToDeclareToCentipede = 65536;
__attribute__((used,
               retain,
               section("__centipede_extra_features"))) static uint64_t
    extra_features[kNumControlsToDeclareToCentipede];
constexpr uint64_t kControlsReachedDomain = 0;

// This fuzzer attempts to explore the space of Chromium UI controls using
// the ATSPI Linux accessibility API. The hope is that virtually all Chromium
// controls are accessible via this API and thus all possible UI interactions
// can be explored (at least in future when this fuzzer gets a bit more
// sophisticated about including more complex HTML pages and/or taking actions
// such as typing text.)
//
// To see the space of controls which the fuzzer explores, either use the
// 'accerciser' GUI tool or build the Chromium `ax_dump_tree` utility.
// (The latter doesn't show so much information but with a few code tweaks
// you can use base::Value::DebugString to get much more out.)
//
// This fuzzer takes pains to use the _names_ of controls wherever possible,
// rather than ordinals. This should yield more stable test cases which may
// allow fuzzing infrastructure to test on different Chromium versions to
// determine regression or fix ranges (subject to the caveats listed below
// about this fuzzer's inability to reset UI state right now.)
// Also, the initial layers of single-child controls are skipped, and that could
// theoretically reduce test case stability if the nature of those first
// layers change.
//
// See the discussion about the custom mutator to see the main cost of
// identifying controls by name.
class AtspiInProcessFuzzer
    : public InProcessProtoFuzzer<test::fuzzing::atspi_fuzzing::FuzzCase> {
 public:
  AtspiInProcessFuzzer();
  void SetUpOnMainThread() override;

  int Fuzz(const test::fuzzing::atspi_fuzzing::FuzzCase& fuzz_case) override;

  static size_t CustomMutator(uint8_t* data,
                              size_t size,
                              size_t max_size,
                              unsigned int seed);

 private:
  void LoadAPage();
  static ScopedAtspiAccessible GetRootNode();
  static std::vector<ScopedAtspiAccessible> GetChildren(
      ScopedAtspiAccessible& node);
  static std::string GetNodeName(const ScopedAtspiAccessible& node);
  static std::string GetNodeRole(const ScopedAtspiAccessible& node);
  static bool InvokeAction(ScopedAtspiAccessible& node, size_t action_id);
  static bool ReplaceText(ScopedAtspiAccessible& node,
                          const std::string& newtext);
  static bool SetSelection(ScopedAtspiAccessible& node,
                           const std::vector<uint32_t>& new_selection);
  // Checks an ATSPI return value and indicates whether to return early
  static bool CheckOk(gboolean ok, GError** error);
  // Checks an ATSPI return value from a function that returns a string;
  // returns either the string or a blank string
  static std::string CheckString(char* result, GError** error);
  static std::optional<size_t> FindMatchingControl(
      const std::vector<ScopedAtspiAccessible>& controls,
      const test::fuzzing::atspi_fuzzing::PathElement& selector);
  static void RecordChildrenForUseByMutator(
      const std::vector<ScopedAtspiAccessible>& children);

  static size_t MutateUsingLPM(uint8_t* data,
                               size_t size,
                               size_t max_size,
                               unsigned int seed);

  static std::optional<size_t> MutateUsingNameAndRole(uint8_t* data,
                                                      size_t size,
                                                      size_t max_size,
                                                      std::minstd_rand& random);
  static bool AttemptMutateMessage(AtspiInProcessFuzzer::FuzzCase& message,
                                   std::minstd_rand& random);
};

// The following is the equivalent of
// REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER(AtspiInProcessFuzzer) but doesn't
// include the standard libprotobuf mutator mutation function, because
// we define our own at the bottom of this file. So we have to explode
// the macro to exclude the standard mutator.
REGISTER_IN_PROCESS_FUZZER(AtspiInProcessFuzzer)
static void TestOneProtoInput(AtspiInProcessFuzzer::FuzzCase);
using FuzzerProtoType =
    protobuf_mutator::libfuzzer::macro_internal::GetFirstParam<
        decltype(&TestOneProtoInput)>::type;
DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(false, FuzzerProtoType)
DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(FuzzerProtoType)

// An on-disk database of all known control names and roles we have
// encountered. These are filled in by the fuzzer then consumed by the
// mutator. We store these on disk because in centipede, the fuzzer
// and mutator run in different invocations of this process.
// For libfuzzer, this complexity wouldn't be needed and we could
// just keep this list in RAM.
class Database {
 public:
  static Database* GetInstance();

  std::optional<std::string> GetRandomRole(std::minstd_rand& random);
  std::optional<std::string> GetRandomName(std::minstd_rand& random);

  void InsertName(const std::string& name);
  void InsertRole(const std::string& role);

 private:
  Database();
  friend struct base::DefaultSingletonTraits<Database>;

  std::optional<std::string> GetRandomValue(const std::string& table_name,
                                            const std::string& column_name,
                                            std::minstd_rand& random,
                                            sql::StatementID statement_id);
  void DoInsert(const std::string& table_name,
                const std::string& value,
                sql::StatementID statement_id);

  std::unique_ptr<sql::Database> db_;
};

AtspiInProcessFuzzer::AtspiInProcessFuzzer() {
  // For some reason when running as Chromium rather than an official build,
  // our accessibility subsystem gets told "no" by D-Bus when querying whether
  // it should enable accessibility. This overrides that.
  setenv("ACCESSIBILITY_ENABLED", "1", 1);
}

void AtspiInProcessFuzzer::SetUpOnMainThread() {
  InProcessProtoFuzzer<
      test::fuzzing::atspi_fuzzing::FuzzCase>::SetUpOnMainThread();
  LoadAPage();
  // LoadAPage will wait until the load event has completed, but we also
  // want to wait until the browser has had time to draw its complete UI
  // and generally get ready to accept input events, so RunUntilIdle as well.
  base::RunLoop().RunUntilIdle();
}

void AtspiInProcessFuzzer::LoadAPage() {
  // Placeholder content with some form controls
  // In the future we might want to experiment with more complex pages
  // here.
  std::string html_string =
      "<html><head><title>Test</title></head><body><form>Username: <input "
      "name=\"username\" type=\"text\">Password: "
      "<input name=\"password\" type=\"password\"><input name=\"Submit\" "
      "type=\"submit\"></form></body></html>";
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(html_string, kUsePlus));
  CHECK(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
}

int AtspiInProcessFuzzer::Fuzz(
    const test::fuzzing::atspi_fuzzing::FuzzCase& fuzz_case) {
  size_t control_path_id = 0;
  // Immediately reject cases where any name or role isn't a valid string,
  // instead of wasting time handling some of their actions.
  // We specifically reject \0 characters as this can cause crashes.
  for (const test::fuzzing::atspi_fuzzing::Action& action :
       fuzz_case.action()) {
    for (const test::fuzzing::atspi_fuzzing::PathElement& path_element :
         action.path_to_control()) {
      switch (path_element.element_type_case()) {
        case test::fuzzing::atspi_fuzzing::PathElement::kNamed: {
          const std::string& name = path_element.named().name();
          if (name.empty() || name.find('\0') != std::string::npos ||
              !base::IsStringUTF8(name)) {
            return -1;
          }
        } break;
        case test::fuzzing::atspi_fuzzing::PathElement::kAnonymous: {
          const std::string& role = path_element.anonymous().role();
          if (role.empty() || role.find('\0') != std::string::npos ||
              !base::IsStringUTF8(role)) {
            return -1;
          }
        } break;
        default:
          break;
      }
    }
  }

  for (const test::fuzzing::atspi_fuzzing::Action& action :
       fuzz_case.action()) {
    // We make no attempt to reset the UI of the browser to any 'starting
    // position', because we can't - we don't know what controls have been
    // explored or what state the browser is in. This is problematic
    // because if a series of test cases are run, the crashing state may
    // only be reached by the concatenated actions of all those cases.
    // At the moment, the behavior of centipede is this:
    // * if it can reproduce a crash with a single test case, it reports
    //   that test case
    // * otherwise, it reports the series of test cases.
    // In the future, it would be even better if:
    // * this fuzzer exposed some (hypothetical) LLVMFuzzerConcatenateCases
    //   function which emits a protobuf of all the actions combined;
    // * ClusterFuzz and centipede are smart enough to apply minimization to
    //   that combined case.
    // This is https://issues.chromium.org/issues/344606392.
    // We're nowhere near that, and we'd only want to consider doing anything
    // along those lines if this fuzzer finds lots of bugs.
    // Enumerate available controls after each action we take - obviously,
    // clicking on one button may make more buttons available
    ScopedAtspiAccessible current_control = GetRootNode();
    // Drill immediately down to the first level which has a choice of controls.
    // The topmost layers each have one child and are the outermost
    // application, which remains the same. (Worse, the outermost control
    // has a name which varies based on RAM usage, so our fuzzer would struggle
    // to make stable test cases.)
    std::vector<ScopedAtspiAccessible> children = GetChildren(current_control);
    while (children.size() == 1) {
      current_control = children[0];
      children = GetChildren(current_control);
    }

    // Keep a record of the control path so we can inform centipede
    std::vector<size_t> current_control_path;
    for (const test::fuzzing::atspi_fuzzing::PathElement& path_element :
         action.path_to_control()) {
      RecordChildrenForUseByMutator(children);
      std::optional<size_t> selected_control =
          FindMatchingControl(children, path_element);
      if (!selected_control.has_value()) {
        return -1;
      }
      current_control = children[*selected_control];
      current_control_path.push_back(*selected_control);

      // Inform centipede of the control path we've reached.
      // We give it a hash of the ordinal path to the control - this doesn't
      // need to be stable across Chromium versions. Each time we
      // declare a new hash here, centipede will know that this is an
      // especially interesting input.
      if (control_path_id < kNumControlsToDeclareToCentipede) {
        base::span<uint8_t> path_data(
            reinterpret_cast<uint8_t*>(current_control_path.data()),
            current_control_path.size() * sizeof(size_t));
        size_t hash =
            base::FastHash(path_data) & std::numeric_limits<uint32_t>::max();
        extra_features[control_path_id++] =
            (kControlsReachedDomain << 32) | hash;
      }

      children = GetChildren(current_control);
    }
    RecordChildrenForUseByMutator(children);

    // We have now chosen a control with which we'll interact during
    // this action
    std::string control_name = GetNodeName(current_control);
    if (kBlockedControls.contains(control_name)) {
      return -1;  // don't explore this case further
    }

    switch (action.action_choice_case()) {
      case test::fuzzing::atspi_fuzzing::Action::kTakeAction:
        if (!InvokeAction(current_control, action.take_action().action_id())) {
          return -1;
        }
        break;
      case test::fuzzing::atspi_fuzzing::Action::kReplaceText:
        if (!ReplaceText(current_control, action.replace_text().new_text())) {
          return -1;
        }
        break;
      case test::fuzzing::atspi_fuzzing::Action::kSetSelection: {
        std::vector<uint32_t> new_selection(
            action.set_selection().selected_child().begin(),
            action.set_selection().selected_child().end());
        if (!SetSelection(current_control, new_selection)) {
          return -1;
        }
      } break;
      case test::fuzzing::atspi_fuzzing::Action::ACTION_CHOICE_NOT_SET:
        break;
    }

    if (action.wait_afterwards()) {
      // Sometimes we might not want to; e.g. to find race conditions
      base::RunLoop().RunUntilIdle();
    }
  }

  return 0;
}

void AtspiInProcessFuzzer::RecordChildrenForUseByMutator(
    const std::vector<ScopedAtspiAccessible>& children) {
  for (auto& child : children) {
    std::string name = GetNodeName(child);
    if (!name.empty() && !kBlockedControls.contains(name)) {
      Database::GetInstance()->InsertName(name);
    }
    std::string role = GetNodeRole(child);
    Database::GetInstance()->InsertRole(role);
  }
}

ScopedAtspiAccessible AtspiInProcessFuzzer::GetRootNode() {
  __pid_t pid = getpid();
  int selectors = ui::AXTreeSelector::None;

  AtspiAccessible* accessible = ui::FindAccessible(ui::AXTreeSelector(
      selectors, "", static_cast<gfx::AcceleratedWidget>(pid)));
  CHECK(accessible);
  return WrapGObject(accessible);
}

std::vector<ScopedAtspiAccessible> AtspiInProcessFuzzer::GetChildren(
    ScopedAtspiAccessible& node) {
  std::vector<ScopedAtspiAccessible> children;

  GError* error = nullptr;
  // Enumerating the attributes seems to be necessary in order for
  // atspi_accessible_get_child_count and atspi_accessible_get_child_at_index
  // to work. Discovered empirically.
  GHashTable* attributes = atspi_accessible_get_attributes(node, &error);
  if (!error && attributes) {
    GHashTableIter i;
    void* key = nullptr;
    void* value = nullptr;

    g_hash_table_iter_init(&i, attributes);
    while (g_hash_table_iter_next(&i, &key, &value)) {
    }
  }
  g_clear_error(&error);
  g_hash_table_unref(attributes);

  // The following code is similar to ui::ChildrenOf, except that we
  // return a vector containing smart pointers which does appropriate reference
  // counting.
  int child_count = atspi_accessible_get_child_count(node, &error);
  if (error) {
    g_clear_error(&error);
    return children;
  }
  if (child_count <= 0) {
    return children;
  }

  for (int i = 0; i < child_count; i++) {
    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(node, i, &error);
    if (error) {
      g_clear_error(&error);
      continue;
    }
    if (child) {
      children.push_back(WrapGObject(child));
    }
  }
  return children;
}

bool AtspiInProcessFuzzer::CheckOk(gboolean ok, GError** error) {
  if (*error) {
    g_clear_error(error);
    return false;
  }
  return ok;
}

std::string AtspiInProcessFuzzer::CheckString(char* result, GError** error) {
  std::string retval;
  if (!*error) {
    retval = result;
  }
  g_clear_error(error);
  free(result);
  return retval;
}

std::string AtspiInProcessFuzzer::GetNodeName(
    const ScopedAtspiAccessible& node) {
  GError* error = nullptr;
  return CheckString(atspi_accessible_get_name(
                         const_cast<ScopedAtspiAccessible&>(node), &error),
                     &error);
}

std::string AtspiInProcessFuzzer::GetNodeRole(
    const ScopedAtspiAccessible& node) {
  GError* error = nullptr;
  return CheckString(atspi_accessible_get_role_name(
                         const_cast<ScopedAtspiAccessible&>(node), &error),
                     &error);
}

bool AtspiInProcessFuzzer::InvokeAction(ScopedAtspiAccessible& node,
                                        size_t action_id) {
  AtspiAction* action = atspi_accessible_get_action_iface(node);
  if (!action) {
    return false;
  }
  GError* error = nullptr;
  size_t num_actions = atspi_action_get_n_actions(action, &error);
  if (error) {
    g_clear_error(&error);
    return false;
  }
  if (num_actions == 0) {
    return false;
  }
  gboolean ok = atspi_action_do_action(action, action_id % num_actions, &error);
  return CheckOk(ok, &error);
}

bool AtspiInProcessFuzzer::ReplaceText(ScopedAtspiAccessible& node,
                                       const std::string& newtext) {
  AtspiEditableText* editable = atspi_accessible_get_editable_text_iface(node);
  if (!editable) {
    return false;
  }
  GError* error = nullptr;
  gboolean ok =
      atspi_editable_text_set_text_contents(editable, newtext.data(), &error);
  return CheckOk(ok, &error);
}

bool AtspiInProcessFuzzer::SetSelection(
    ScopedAtspiAccessible& node,
    const std::vector<uint32_t>& new_selection) {
  AtspiSelection* selection = atspi_accessible_get_selection_iface(node);
  if (!selection) {
    return false;
  }
  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(node, &error);
  if (!CheckOk(error != nullptr, &error)) {
    return false;
  }
  if (child_count == 0) {
    return false;
  }
  std::set<uint32_t> children_to_select;
  for (uint32_t id : new_selection) {
    children_to_select.insert(id % child_count);
  }
  gboolean ok = atspi_selection_clear_selection(selection, &error);
  if (!CheckOk(ok, &error)) {
    return false;
  }
  for (auto idx : children_to_select) {
    ok = atspi_selection_select_child(selection, idx, &error);
    if (!CheckOk(ok, &error)) {
      return false;
    }
  }
  return true;
}

std::optional<size_t> AtspiInProcessFuzzer::FindMatchingControl(
    const std::vector<ScopedAtspiAccessible>& controls,
    const test::fuzzing::atspi_fuzzing::PathElement& selector) {
  // Select the child which matches the selector.
  // Avoid using hash maps or anything fancy, because we want fuzzing engines
  // to be able to instrument the string comparisons here.
  switch (selector.element_type_case()) {
    case test::fuzzing::atspi_fuzzing::PathElement::kNamed: {
      for (size_t i = 0; i < controls.size(); i++) {
        auto& control = controls[i];
        std::string name = GetNodeName(control);
        // Use of .data() below is a workaround for
        // https://issues.chromium.org/issues/343801371
        if (name == selector.named().name().data()) {
          return i;
        }
      }
      break;
    }
    case test::fuzzing::atspi_fuzzing::PathElement::kAnonymous: {
      size_t to_skip = selector.anonymous().ordinal();
      for (size_t i = 0; i < controls.size(); i++) {
        auto& control = controls[i];
        std::string name = GetNodeName(control);
        // Controls with a name MUST be selected by that name,
        // so the fuzzer creates test cases which are maximally stable
        // across Chromium versions. So disregard named controls here.
        if (name == "") {
          // If the control is anonymous, we allow it to be selected
          // by role name and by an ordinal.
          // Such test cases will be less stable, but a lot of controls are
          // nested within anonymous panels and frames - quite often, there's
          // exactly one child control, so test cases should be fairly stable.
          std::string role = GetNodeRole(control);
          // Use of .data() below is a workaround for
          // https://issues.chromium.org/issues/343801371
          if (role == selector.anonymous().role().data()) {
            if (to_skip-- == 0) {
              return i;
            }
          }
        }
      }
      break;
    }
    case test::fuzzing::atspi_fuzzing::PathElement::ELEMENT_TYPE_NOT_SET:
      break;
  }
  return std::nullopt;
}

namespace {

// This stuff is inherited from libprotobuf-mutator and simplified a little.
// It's not exposed as APIs from libprotobuf-mutator so we can't use
// it without violating checkdeps rules, etc.

bool ParseTextMessage(base::span<uint8_t> data,
                      AtspiInProcessFuzzer::FuzzCase* output) {
  std::string data_string = {reinterpret_cast<const char*>(data.data()),
                             data.size()};
  output->Clear();
  google::protobuf::TextFormat::Parser parser;
  parser.SetRecursionLimit(100);
  parser.AllowPartialMessage(true);
  parser.AllowUnknownField(true);
  if (!parser.ParseFromString(data_string, output)) {
    output->Clear();
    return false;
  }
  return true;
}

size_t SaveMessageAsText(const AtspiInProcessFuzzer::FuzzCase& message,
                         uint8_t* data,
                         size_t max_size) {
  std::string tmp;
  if (!google::protobuf::TextFormat::PrintToString(message, &tmp)) {
    return 0;
  }
  if (tmp.size() <= max_size) {
    memcpy(data, tmp.data(), tmp.size());
    return tmp.size();
  }
  return 0;
}

}  // namespace

size_t AtspiInProcessFuzzer::MutateUsingLPM(uint8_t* data,
                                            size_t size,
                                            size_t max_size,
                                            unsigned int seed) {
  AtspiInProcessFuzzer::FuzzCase input;
  return protobuf_mutator::libfuzzer::CustomProtoMutator(
      false, data, size, max_size, seed, &input);
}

// Returns nullopt if we don't successfully mutate this
std::optional<size_t> AtspiInProcessFuzzer::MutateUsingNameAndRole(
    uint8_t* data,
    size_t size,
    size_t max_size,
    std::minstd_rand& random) {
  AtspiInProcessFuzzer::FuzzCase input;
  base::span<uint8_t> message_data(data, size);
  if (!ParseTextMessage(message_data, &input)) {
    return std::nullopt;
  }
  if (AttemptMutateMessage(input, random)) {
    return SaveMessageAsText(input, data, max_size);
  }
  return std::nullopt;
}

// Returns false if we don't successfully mutate this
bool AtspiInProcessFuzzer::AttemptMutateMessage(
    AtspiInProcessFuzzer::FuzzCase& input,
    std::minstd_rand& random) {
  if (input.action_size() == 0) {
    return false;
  }

  // About 50% of the time, choose the last action to mutate
  size_t chosen_action =
      std::uniform_int_distribution<size_t>(0, input.action_size() * 2)(random);
  test::fuzzing::atspi_fuzzing::Action* action = input.mutable_action(
      std::min(chosen_action, static_cast<size_t>(input.action_size()) - 1));
  if (action->path_to_control_size() == 0) {
    return false;
  }
  // Some of the time, add another path element (so we reach into
  // deeper controls)
  if (std::uniform_int_distribution<size_t>(0, 2)(random) > 1) {
    action->add_path_to_control();
  }

  // About 50% of the time, choose the last path element to mutate
  size_t chosen_path_element = std::uniform_int_distribution<int64_t>(
      0, action->path_to_control_size() * 2)(random);
  test::fuzzing::atspi_fuzzing::PathElement* path_element =
      action->mutable_path_to_control(
          std::min(chosen_path_element,
                   static_cast<size_t>(action->path_to_control_size()) - 1));
  // Sometimes, switch anonymous elements to named
  if (path_element->has_named() ||
      std::uniform_int_distribution<size_t>(0, 2)(random) > 1) {
    std::optional<std::string> name =
        Database::GetInstance()->GetRandomName(random);
    if (!name.has_value()) {
      return false;
    }
    if (*name == path_element->named().name()) {
      return false;
    }
    *path_element->mutable_named()->mutable_name() = *name;
  } else {
    std::optional<std::string> role =
        Database::GetInstance()->GetRandomRole(random);
    if (!role.has_value()) {
      return false;
    }
    if (*role == path_element->anonymous().role()) {
      return false;
    }
    *path_element->mutable_anonymous()->mutable_role() = *role;
  }
  return true;
}

size_t AtspiInProcessFuzzer::CustomMutator(uint8_t* data,
                                           size_t size,
                                           size_t max_size,
                                           unsigned int seed) {
  std::minstd_rand random(seed);

  // 0 = use just libprotobuf-mutator
  // 1 = use libprotobuf-mutator then our mutator
  //     (sometimes this might be useful for instance to get from "panel 2"
  //     to "frame 3", or something. "panel 3" might not be a valid control.)
  // 2-6 = use just our mutator
  int mutation_strategy = std::uniform_int_distribution(0, 6)(random);

  switch (mutation_strategy) {
    case 0:
      return MutateUsingLPM(data, size, max_size, random());
    case 1: {
      size = MutateUsingLPM(data, size, max_size, random());
      std::optional<size_t> new_size =
          MutateUsingNameAndRole(data, size, max_size, random);
      return new_size.value_or(size);
    }
    default: {
      std::optional<size_t> new_size =
          MutateUsingNameAndRole(data, size, max_size, random);
      if (!new_size.has_value()) {
        return MutateUsingLPM(data, size, max_size, random());
      }
      return *new_size;
    }
  }
}

// A custom mutator which sometimes uses the standard libprotobuf-mutator,
// but may alternatively mutate the input to use a known-valid name or role.
// We do it this way instead of using lpm's post-mutation validation
// because post-mutation validation is not permitted to affect valid
// test cases.
//
// STRATEGY:
//
// We want this fuzzer to produce stable test cases, so the protobufs need to
// refer to test cases by name, instead of by ordinal, wherever possible.
// Of course, the vast majority of strings are not valid control names which
// happen to exist at the right point in the tree, and therefore it would take
// nearly infinite time to stumble across the right control names.
//
// This is somewhat shortcutted by the string comparison instrumentation
// feeding back known strings into libfuzzer's table of recent comparisons.
// This does allow the fuzzer to make progress, but it's still extremely slow,
// despite the FindMatchingControl function being structured to allow this.
// (https://issues.chromium.org/issues/346918512 probably doesn't help).
//
// We therefore sometimes use this custom mutator to specify control names
// which are known to actually exist. This is pushing our luck a little -
// the list of known control names will vary depending on what test cases
// have already been run, and therefore this mutator isn't guaranteed to
// mutate a test case the same way each time for a given seed. That's
// probably bad, but not as bad as the lack of determinism caused by UI
// state within the actual fuzzer, so it seems a small price to pay. And
// it is effective - it enables the fuzzer to reach into controls in a
// fairly rapid fashion, while still using control names within the test
// cases wherever possible.
//
// CENTIPEDE: Unfortunately, in centipede, the mutator runs in a different
// invocation of the process than the actual fuzzer. The custom mutator
// therefore has no access to the real control names and roles which have
// been discovered, and always falls back to using the regular LPM mutator.
// This makes the fuzzer significantly less effective. In the future
// we could work around this by persisting the control names to disk, or
// similar.
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data,
                                          size_t size,
                                          size_t max_size,
                                          unsigned int seed) {
  return AtspiInProcessFuzzer::CustomMutator(data, size, max_size, seed);
}

Database* Database::GetInstance() {
  return base::Singleton<Database>::get();
}

Database::Database() {
  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = false,  // centipede may run several fuzzers at once
      .page_size = sql::DatabaseOptions::kDefaultPageSize,
      .cache_size = 0,
  });
  base::FilePath db_path;
  CHECK(base::PathService::Get(base::DIR_TEMP, &db_path));
  db_path = db_path.AppendASCII("atspi_in_process_fuzzer_controls.db");
  CHECK(db_->Open(db_path));
  if (!db_->DoesTableExist("roles")) {
    CHECK(db_->Execute("create table roles (role TEXT NOT NULL UNIQUE)"));
  }
  if (!db_->DoesTableExist("names")) {
    CHECK(db_->Execute("create table names (name TEXT NOT NULL UNIQUE)"));
  }
}

void Database::InsertName(const std::string& name) {
  DoInsert("names", name, SQL_FROM_HERE);
}

void Database::InsertRole(const std::string& role) {
  DoInsert("roles", role, SQL_FROM_HERE);
}

void Database::DoInsert(const std::string& table_name,
                        const std::string& value,
                        sql::StatementID statement_id) {
  std::string insert_sql = base::StringPrintf(
      "INSERT OR IGNORE INTO %s VALUES (?)", table_name.c_str());
  sql::Statement stmt(db_->GetCachedStatement(statement_id, insert_sql));
  stmt.BindString(0, value);
  base::IgnoreResult(stmt.Run());  // ignore result in case other instances
                                   // of the fuzzer have the database locked
}

std::optional<std::string> Database::GetRandomRole(std::minstd_rand& random) {
  return GetRandomValue("roles", "role", random, SQL_FROM_HERE);
}

std::optional<std::string> Database::GetRandomName(std::minstd_rand& random) {
  return GetRandomValue("names", "name", random, SQL_FROM_HERE);
}

std::optional<std::string> Database::GetRandomValue(
    const std::string& table_name,
    const std::string& column_name,
    std::minstd_rand& random,
    sql::StatementID statement_id) {
  size_t random_selector =
      std::uniform_int_distribution<int64_t>(INT64_MIN, INT64_MAX)(random);
  std::string get_query = base::StringPrintf(
      "select %s from %s limit 1 offset (? %% (SELECT COUNT(*) FROM %s))",
      column_name.c_str(), table_name.c_str(), table_name.c_str());
  sql::Statement get_statement(
      db_->GetCachedStatement(statement_id, get_query));
  get_statement.BindInt64(0, random_selector);
  if (!get_statement.Step()) {
    return std::nullopt;
  }
  return get_statement.ColumnString(0);
}
