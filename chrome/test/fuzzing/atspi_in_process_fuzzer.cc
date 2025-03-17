// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <atspi/atspi-types.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "base/base_paths.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/atspi_in_process_fuzzer.pb.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/base/glib/scoped_gobject.h"

// Controls (by name) which we shouldn't choose.
constexpr auto kBlockedControls =
    base::MakeFixedFlatSet<std::string_view>({"Close", "Relaunch"});

// When developing this fuzzer, it's really useful to have this logging,
// but it's too verbose for normal running (it can mask crash information).
#if 0
#define ATSPI_FUZZER_LOG LOG(INFO)
#else
#define ATSPI_FUZZER_LOG EAT_STREAM_PARAMETERS
#endif

using ScopedAtspiAccessible = ScopedGObject<AtspiAccessible>;

using test::fuzzing::atspi_fuzzing::Action;
using test::fuzzing::atspi_fuzzing::ActionVerb;
using test::fuzzing::atspi_fuzzing::ControlPath;
using test::fuzzing::atspi_fuzzing::PathElement;

// We inform centipede of control paths we've explored, to
// bias centipede towards exploring new controls.
static constexpr size_t kNumControlsToDeclareToCentipede = 65536;
__attribute__((used,
               retain,
               section("__centipede_extra_features"))) static uint64_t
    extra_features[kNumControlsToDeclareToCentipede];
constexpr uint64_t kControlsReachedDomain = 0;

enum NodeDepth { Root, FirstLevel, Other };

// A node we've found in the UI, and all its children and grand children.
class UiNode {
 public:
  explicit UiNode(ScopedAtspiAccessible accessible, NodeDepth depth);
  std::string& GetName() const;  // returns "" if no name
  std::string& GetRole() const;  // returns "" if no role
  bool IsBlocklisted() const;
  std::optional<size_t> FindMatchingChild(const PathElement& selector) const;
  UiNode* GetNthChild(size_t index) { return children_[index].get(); }
  void RescanAndFindNewChildren(std::vector<std::vector<UiNode*>>& new_controls,
                                const std::vector<UiNode*> my_path);
  void GetAllChildren(std::vector<std::vector<UiNode*>>& new_controls,
                      const std::vector<UiNode*> my_path);
  void AppendControlPath(ControlPath& output_path,
                         const base::span<UiNode*>& path_to_control);
  ScopedAtspiAccessible& Get() { return node_; }
  bool ContainsPasswordNode() const;
  // A clue about whether this is likely to yield useful activity if
  // we poke at it.
  bool ProbablyActionable() const;

 private:
  void ScanAttributes() const;
  mutable ScopedAtspiAccessible
      node_;  // needs to be mutable to allow retrieving name and role
  NodeDepth depth_;
  mutable std::optional<std::string> name_;        // populated on demand
  mutable std::optional<std::string> role_;        // populated on demand
  std::vector<std::unique_ptr<UiNode>> children_;  // populated on construction
};

// This fuzzer attempts to explore the space of Chromium UI controls using
// the ATSPI Linux accessibility API. The hope is that virtually all Chromium
// controls are accessible via this API and thus all possible UI interactions
// can be explored (at least in future when this fuzzer gets a bit more
// sophisticated about including more complex HTML pages.)
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
    : public InProcessTextProtoFuzzer<test::fuzzing::atspi_fuzzing::FuzzCase> {
 public:
  AtspiInProcessFuzzer();
  void SetUpOnMainThread() override;

  bool UseSingleProcessMode() override;
  int Fuzz(const test::fuzzing::atspi_fuzzing::FuzzCase& fuzz_case) override;

  static size_t CustomMutator(uint8_t* data,
                              size_t size,
                              size_t max_size,
                              unsigned int seed);

 private:
  void LoadAPage();
  int HandleAction(const Action& action, size_t& control_path_id);
  static ScopedAtspiAccessible GetRootNode();
  static bool InvokeAction(ScopedAtspiAccessible& node, size_t action_id);
  static bool ReplaceText(ScopedAtspiAccessible& node,
                          const std::string& newtext);
  static bool SetSelection(ScopedAtspiAccessible& node,
                           const std::vector<uint32_t>& new_selection);
  std::string StringifyNodePath(const ControlPath& path);
  static std::string DebugPath(const ControlPath& path);

  // Mutation related code
  static size_t MutateUsingLPM(uint8_t* data,
                               size_t size,
                               size_t max_size,
                               unsigned int seed);

  static std::optional<size_t> MutateControlPath(uint8_t* data,
                                                 size_t size,
                                                 size_t max_size,
                                                 std::minstd_rand& random);
  static void AddPrerequisiteActionToTestCase(
      google::protobuf::TextFormat::Parser& parser,
      AtspiInProcessFuzzer::FuzzCase& input,
      const std::string& control_path,
      size_t position_to_insert,
      std::minstd_rand& random,
      size_t overflow_guard);
  static bool AttemptMutateMessage(AtspiInProcessFuzzer::FuzzCase& message,
                                   std::minstd_rand& random);

  // Initialized in SetupOnMainThread, then valid thereafter
  std::optional<UiNode> ui_state_;
  // If we're in -merge mode, skip enumerating UI controls because it's
  // too slow and merges time out.
  bool merge_mode_ = false;
};

// Stringified version of Action in the protobuf.
// For hashing and passing to database.
struct ActionPath {
  std::string control_path;
  std::string verb_string;
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

// An on-disk database of all known control paths which we have
// encountered. These are filled in by the fuzzer then consumed by the
// mutator. We store these on disk because in centipede, the fuzzer
// and mutator run in different invocations of this process.
// For libfuzzer, this complexity wouldn't be needed and we could
// just keep this list in RAM.
class Database {
 public:
  static Database* GetInstance();

  std::optional<std::string> GetRandomControlPath(std::minstd_rand& random);
  std::optional<ActionPath> GetPrerequisite(const std::string& control,
                                            std::minstd_rand& random);
  void InsertControlPathAndPrerequisites(
      const std::string& newly_visible_control,
      const std::optional<ActionPath>& prerequisite_action,
      bool probably_actionable);

 private:
  Database();
  void DropTableIfExists(const std::string& table);
  std::optional<int64_t> InsertControlPath(const std::string& control_path,
                                           bool probably_actionable,
                                           bool has_prereq);
  friend struct base::DefaultSingletonTraits<Database>;

  std::unique_ptr<sql::Database> db_;
};

AtspiInProcessFuzzer::AtspiInProcessFuzzer() {
  // For some reason when running as Chromium rather than an official build,
  // our accessibility subsystem gets told "no" by D-Bus when querying whether
  // it should enable accessibility. This overrides that.
  setenv("ACCESSIBILITY_ENABLED", "1", 1);
}

bool AtspiInProcessFuzzer::UseSingleProcessMode() {
  return false;
}

void AtspiInProcessFuzzer::SetUpOnMainThread() {
  InProcessTextProtoFuzzer<
      test::fuzzing::atspi_fuzzing::FuzzCase>::SetUpOnMainThread();
  LoadAPage();
  // LoadAPage will wait until the load event has completed, but we also
  // want to wait until the browser has had time to draw its complete UI
  // and generally get ready to accept input events, so we'll keep looping
  // here until we see a control called Password, indicating that the
  // accessibility tree corresponding to our web page has appeared.
  ATSPI_FUZZER_LOG << "Waiting for AX tree to be populated";
  size_t counter = 0;
  bool found_ui = false;
  while (!found_ui) {
    base::RunLoop nested_run_loop;
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(100),
                nested_run_loop.QuitClosure());
    nested_run_loop.Run();
    ScopedAtspiAccessible root_node = GetRootNode();
    ui_state_ = UiNode(root_node, NodeDepth::Root);  // explores UI
    if (ui_state_->ContainsPasswordNode()) {
      found_ui = true;
      break;
    }
    CHECK(counter++ < 300)
        << "It took more than 30 seconds for the AX tree to be populated";
  }
  // Ensure the database is populated with the controls visible at the
  // outset
  std::vector<std::vector<UiNode*>> all_controls;
  std::vector<UiNode*> root_path;
  ui_state_->GetAllChildren(all_controls, root_path);

  ATSPI_FUZZER_LOG << "AX tree populated; found " << all_controls.size()
                   << " controls.";
  for (auto& control : all_controls) {
    ControlPath path;
    ui_state_->AppendControlPath(path, control);
    ATSPI_FUZZER_LOG << "  " << DebugPath(path);
    Database::GetInstance()->InsertControlPathAndPrerequisites(
        StringifyNodePath(path), std::nullopt,
        control.back()->ProbablyActionable());
  }
  ATSPI_FUZZER_LOG << "Initial controls inserted into database.";
  merge_mode_ = InMergeMode();
  LOG(INFO) << "Merging mode: " << merge_mode_;
}

std::string AtspiInProcessFuzzer::DebugPath(const ControlPath& path) {
  std::stringstream ss;
  for (auto& elem : path.path_to_control()) {
    if (elem.has_named()) {
      ss << '"' << elem.named().name() << '"';
    } else {
      ss << '"' << elem.anonymous().role()
         << "\":" << elem.anonymous().ordinal();
    }
    ss << ", ";
  }
  std::string output = ss.str();
  if (!output.empty()) {
    output.resize(output.size() - 2);  // strip last comma and space
  }
  return output;
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
         action.path_to_control().path_to_control()) {
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
          return -1;
      }
    }
    if (action.verb().action_choice_case() ==
        test::fuzzing::atspi_fuzzing::ActionVerb::ACTION_CHOICE_NOT_SET) {
      return -1;
    }
  }

  for (const test::fuzzing::atspi_fuzzing::Action& action :
       fuzz_case.action()) {
    int status = HandleAction(action, control_path_id);

    if (status != 0) {
      return status;
    }
  }

  return 0;
}

int AtspiInProcessFuzzer::HandleAction(
    const test::fuzzing::atspi_fuzzing::Action& action,
    size_t& control_path_id) {
  UiNode* current_control = &*ui_state_;

  // Keep a record of the control path so we can inform centipede
  std::vector<size_t> current_control_path;
  for (const test::fuzzing::atspi_fuzzing::PathElement& path_element :
       action.path_to_control().path_to_control()) {
    std::optional<size_t> selected_control =
        current_control->FindMatchingChild(path_element);
    if (!selected_control.has_value()) {
      ATSPI_FUZZER_LOG << "Failed to find "
                       << DebugPath(action.path_to_control());
      // This control should have been visible in the UI, because we
      // first try to run any pre-requisite steps to make it visible,
      // but it wasn't. Therefore we assume we've got stuck in some
      // deeper UI state from some previous fuzzing iteration.
      // For centipede (but not libfuzzer) an option here is to bail out
      // using DeclareInfiniteLoop(), which will cause the runner to restart
      // the fuzzer and thus the UI state. However, it turns out we hit this
      // a lot, and it thus reduces iteration speed to a crawl. Instead,
      // on any engine, we'll just power on and hope that UI interactions
      // will happen to reset us to the starting state, or (in the case of
      // centipede) we get restarted soon anyway.
      return -1;
    }
    current_control_path.push_back(*selected_control);
    current_control = current_control->GetNthChild(*selected_control);

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
      extra_features[control_path_id++] = (kControlsReachedDomain << 32) | hash;
    }
  }

  // We have now chosen a control with which we'll interact during
  // this action
  if (current_control->IsBlocklisted()) {
    return -1;  // don't explore this case further
  }

  ATSPI_FUZZER_LOG << "Acting on " << DebugPath(action.path_to_control());
  switch (action.verb().action_choice_case()) {
    case test::fuzzing::atspi_fuzzing::ActionVerb::kTakeAction:
      if (!InvokeAction(current_control->Get(),
                        action.verb().take_action().action_id())) {
        return -1;
      }
      break;
    case test::fuzzing::atspi_fuzzing::ActionVerb::kReplaceText:
      if (!ReplaceText(current_control->Get(),
                       action.verb().replace_text().new_text())) {
        return -1;
      }
      break;
    case test::fuzzing::atspi_fuzzing::ActionVerb::kSetSelection: {
      std::vector<uint32_t> new_selection(
          action.verb().set_selection().selected_child().begin(),
          action.verb().set_selection().selected_child().end());
      if (!SetSelection(current_control->Get(), new_selection)) {
        return -1;
      }
    } break;
    case test::fuzzing::atspi_fuzzing::ActionVerb::ACTION_CHOICE_NOT_SET:
      break;
  }

  base::RunLoop().RunUntilIdle();

  if (merge_mode_) {
    return 0;
  }

  // If new components are visible, record how to reach them for
  // the sake of the mutator in future.
  std::vector<std::vector<UiNode*>> new_controls;
  const std::vector<UiNode*> empty_path;
  ui_state_->RescanAndFindNewChildren(new_controls, empty_path);

  if (new_controls.empty()) {
    return 0;
  }

  ActionPath action_path;
  if (!google::protobuf::TextFormat::PrintToString(action.verb(),
                                                   &action_path.verb_string)) {
    return 0;
  }
  if (!google::protobuf::TextFormat::PrintToString(action.path_to_control(),
                                                   &action_path.control_path)) {
    return 0;
  }

  ATSPI_FUZZER_LOG << "Interacting with " << DebugPath(action.path_to_control())
                   << " made visible:";
  for (auto& node : new_controls) {
    ControlPath node_path;
    ui_state_->AppendControlPath(node_path, node);
    ATSPI_FUZZER_LOG << "  " << DebugPath(node_path);
    Database::GetInstance()->InsertControlPathAndPrerequisites(
        StringifyNodePath(node_path), action_path,
        node.back()->ProbablyActionable());
  }

  return 0;
}

std::string AtspiInProcessFuzzer::StringifyNodePath(
    const test::fuzzing::atspi_fuzzing::ControlPath& path) {
  std::string output_path_string;
  if (!google::protobuf::TextFormat::PrintToString(path, &output_path_string)) {
    return "";  // FIXME
  }
  return output_path_string;
}

ScopedAtspiAccessible AtspiInProcessFuzzer::GetRootNode() {
  __pid_t pid = getpid();
  int selectors = ui::AXTreeSelector::None;

  AtspiAccessible* accessible = ui::FindAccessible(ui::AXTreeSelector(
      selectors, "", static_cast<gfx::AcceleratedWidget>(pid)));
  CHECK(accessible);
  return WrapGObject(accessible);
}

namespace {

// Checks an ATSPI return value and indicates whether to return early
bool CheckOk(gboolean ok, GError** error) {
  if (*error) {
    g_clear_error(error);
    return false;
  }
  return ok;
}

// Checks an ATSPI return value from a function that returns a string;
// returns either the string or a blank string
std::string CheckString(char* result, GError** error) {
  std::string retval;
  if (!*error) {
    retval = result;
  }
  g_clear_error(error);
  free(result);
  return retval;
}

}  // namespace

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

bool UiNode::IsBlocklisted() const {
  return kBlockedControls.contains(GetName());
}

bool UiNode::ContainsPasswordNode() const {
  if (GetName() == "Password: ") {
    return true;
  }
  for (auto& child : children_) {
    if (child->ContainsPasswordNode()) {
      return true;
    }
  }
  return false;
}

bool UiNode::ProbablyActionable() const {
  if (!GetName().empty()) {
    return true;
  }
  std::string& role = GetRole();
  return role != "frame" && role != "panel";
}

std::string& UiNode::GetName() const {
  if (name_.has_value()) {  // cached
    return *name_;
  }
  if (depth_ != NodeDepth::Other) {
    // The root node name varies according to RAM usage. Pretend it has no name
    // so we identify it by role instead.
    name_ = "";
  } else {
    GError* error = nullptr;
    name_ = CheckString(atspi_accessible_get_name(node_, &error), &error);
  }
  return *name_;
}

std::string& UiNode::GetRole() const {
  if (role_.has_value()) {  // cached
    return *role_;
  }

  GError* error = nullptr;
  role_ = CheckString(atspi_accessible_get_role_name(node_, &error), &error);
  return *role_;
}

std::optional<size_t> UiNode::FindMatchingChild(
    const test::fuzzing::atspi_fuzzing::PathElement& selector) const {
  // Select the child which matches the selector.
  // Avoid using hash maps or anything fancy, because we want fuzzing engines
  // to be able to instrument the string comparisons here.
  switch (selector.element_type_case()) {
    case test::fuzzing::atspi_fuzzing::PathElement::kNamed: {
      for (size_t i = 0; i < children_.size(); i++) {
        auto& control = children_[i];
        std::string& name = control->GetName();
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
      for (size_t i = 0; i < children_.size(); i++) {
        auto& control = children_[i];
        std::string name = control->GetName();
        // Controls with a name MUST be selected by that name,
        // so the fuzzer creates test cases which are maximally stable
        // across Chromium versions. So disregard named controls here.
        if (name == "") {
          // If the control is anonymous, we allow it to be selected
          // by role name and by an ordinal.
          // Such test cases will be less stable, but a lot of controls are
          // nested within anonymous panels and frames - quite often, there's
          // exactly one child control, so test cases should be fairly stable.
          std::string& role = control->GetRole();
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

UiNode::UiNode(ScopedAtspiAccessible accessible, NodeDepth depth)
    : node_(accessible), depth_(depth) {
  // Enumerate children immediately on construction of the node.
  // We'll always need to know them.
  ScanAttributes();
  // The following code is similar to ui::ChildrenOf, except that we
  // create a vector of UiNodes instead of raw AtspiAccessible pointers
  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(node_, &error);
  if (error) {
    g_clear_error(&error);
    return;
  }
  if (child_count <= 0) {
    return;
  }
  children_.reserve(child_count);

  NodeDepth next_depth =
      (depth == NodeDepth::Root) ? NodeDepth::FirstLevel : NodeDepth::Other;
  for (int i = 0; i < child_count; i++) {
    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(node_, i, &error);
    if (error) {
      g_clear_error(&error);
      continue;
    }
    if (child) {
      children_.push_back(
          std::make_unique<UiNode>(WrapGObject(child), next_depth));
    }
  }
}

void UiNode::ScanAttributes() const {
  // Enumerating the attributes seems to be necessary in order for
  // atspi_accessible_get_child_count and atspi_accessible_get_child_at_index
  // to work. Discovered empirically.
  GError* error = nullptr;

  GHashTable* attributes = atspi_accessible_get_attributes(node_, &error);
  if (!error && attributes) {
    GHashTableIter i;
    void* key = nullptr;
    void* value = nullptr;

    g_hash_table_iter_init(&i, attributes);
    while (g_hash_table_iter_next(&i, &key, &value)) {
    }
  }
  g_clear_error(&error);
  if (attributes) {
    g_hash_table_unref(attributes);
  }
}

void UiNode::RescanAndFindNewChildren(
    std::vector<std::vector<UiNode*>>& new_controls,
    const std::vector<UiNode*> my_path) {
  // We want to reuse existing UiNode objects where possible so we
  // retain cached information.
  // First make a map of such objects to avoid O(n^2)
  std::unordered_map<AtspiAccessible*, std::unique_ptr<UiNode>*> old_nodes;
  old_nodes.reserve(children_.size());
  for (auto& child : children_) {
    old_nodes.insert(std::make_pair(child->Get(), &child));
  }

  ScanAttributes();

  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(node_, &error);
  if (error) {
    g_clear_error(&error);
    return;
  }
  if (child_count < 0) {
    return;
  }

  std::vector<std::unique_ptr<UiNode>> revised_children;
  revised_children.reserve(child_count);
  NodeDepth next_depth =
      (depth_ == NodeDepth::Root) ? NodeDepth::FirstLevel : NodeDepth::Other;

  for (int i = 0; i < child_count; i++) {
    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(node_, i, &error);
    if (error) {
      g_clear_error(&error);
      continue;
    }
    if (child) {
      auto it = old_nodes.find(child);
      std::vector<UiNode*> new_node_path = my_path;
      if (it == old_nodes.end()) {
        // New node!
        revised_children.push_back(
            std::make_unique<UiNode>(WrapGObject(child), next_depth));
        new_node_path.push_back(revised_children.back().get());
        new_controls.push_back(new_node_path);
        revised_children.back()->GetAllChildren(new_controls, new_node_path);
      } else {
        // Pre-existing node
        revised_children.push_back(std::move(*it->second));
        new_node_path.push_back(revised_children.back().get());
        revised_children.back()->RescanAndFindNewChildren(new_controls,
                                                          new_node_path);
      }
    }
  }
  children_ = std::move(revised_children);
}

void UiNode::GetAllChildren(std::vector<std::vector<UiNode*>>& new_controls,
                            const std::vector<UiNode*> my_path) {
  for (auto& child : children_) {
    std::vector<UiNode*> child_path = my_path;
    child_path.push_back(child.get());
    new_controls.push_back(child_path);
    child->GetAllChildren(new_controls, child_path);
  }
}

void UiNode::AppendControlPath(ControlPath& output_path,
                               const base::span<UiNode*>& path_to_control) {
  UiNode* desired_child = path_to_control.front();
  PathElement* output_element = output_path.add_path_to_control();
  if (!desired_child->GetName().empty()) {
    *output_element->mutable_named()->mutable_name() = desired_child->GetName();
  } else {
    std::string& desired_role = desired_child->GetRole();
    *output_element->mutable_anonymous()->mutable_role() = desired_role;
    size_t prior_controls_with_this_role = 0;
    for (auto& child : children_) {
      std::string& name = child->GetName();
      if (name.empty()) {
        if (child.get() == desired_child) {
          output_element->mutable_anonymous()->set_ordinal(
              prior_controls_with_this_role);
          break;
        }
        if (child->GetRole() == desired_role) {
          prior_controls_with_this_role++;
        }
      }
    }
  }
  if (path_to_control.size() > 1) {
    desired_child->AppendControlPath(output_path, path_to_control.subspan(1u));
  }
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
std::optional<size_t> AtspiInProcessFuzzer::MutateControlPath(
    uint8_t* data,
    size_t size,
    size_t max_size,
    std::minstd_rand& random) {
  AtspiInProcessFuzzer::FuzzCase input;
  base::span<uint8_t> message_data(data, size);
  ParseTextMessage(message_data, &input);
  // If we can't parse it, we'll treat it as a blank fuzz case
  if (AttemptMutateMessage(input, random)) {
    return SaveMessageAsText(input, data, max_size);
  }
  return std::nullopt;
}

void AtspiInProcessFuzzer::AddPrerequisiteActionToTestCase(
    google::protobuf::TextFormat::Parser& parser,
    AtspiInProcessFuzzer::FuzzCase& input,
    const std::string& control_path,
    size_t position_to_insert,
    std::minstd_rand& random,
    size_t overflow_guard) {
  if (overflow_guard > 100) {
    return;
  }
  std::optional<ActionPath> prereq =
      Database::GetInstance()->GetPrerequisite(control_path, random);
  if (prereq) {
    Action* new_action = input.add_action();
    if (!parser.ParseFromString(prereq->control_path,
                                new_action->mutable_path_to_control())) {
      return;
    }
    if (!parser.ParseFromString(prereq->verb_string,
                                new_action->mutable_verb())) {
      return;
    }
    // We added this new prereq at the end of all the actions - move
    // it to just before the action we're mutating.
    google::protobuf::RepeatedPtrField<Action>* action_field(
        input.mutable_action());
    // Reverse iterate, swapping new element in front each time
    for (size_t i(input.action_size() - 1); i > position_to_insert; --i) {
      action_field->SwapElements(i, i - 1);
    }

    // Recurse in case this new action also has pre-requisite actions.
    AddPrerequisiteActionToTestCase(parser, input, prereq->control_path,
                                    position_to_insert, random,
                                    overflow_guard + 1);
  }
}

// Returns false if we don't successfully mutate this
bool AtspiInProcessFuzzer::AttemptMutateMessage(
    AtspiInProcessFuzzer::FuzzCase& input,
    std::minstd_rand& random) {
  if (input.action_size() == 0) {
    test::fuzzing::atspi_fuzzing::Action* action = input.add_action();
    action->mutable_verb()->mutable_take_action();
  }

  // About 50% of the time, choose the last action to mutate
  size_t random_action =
      std::uniform_int_distribution<size_t>(0, input.action_size() * 2)(random);
  size_t chosen_action =
      std::min(random_action, static_cast<size_t>(input.action_size()) - 1);
  test::fuzzing::atspi_fuzzing::Action* action =
      input.mutable_action(chosen_action);

  std::optional<std::string> control_path =
      Database::GetInstance()->GetRandomControlPath(random);
  if (!control_path.has_value()) {
    // Database brand new, doesn't yet know about any controls because
    // we haven't yet run the fuzzer - let the LPM fuzzer invent
    // bobbins for this first run.
    return false;
  }

  action->mutable_path_to_control()->Clear();
  google::protobuf::TextFormat::Parser parser;
  parser.SetRecursionLimit(100);
  parser.AllowPartialMessage(true);
  parser.AllowUnknownField(true);
  if (!parser.ParseFromString(*control_path,
                              action->mutable_path_to_control())) {
    return false;
  }

  AddPrerequisiteActionToTestCase(parser, input, *control_path, chosen_action,
                                  random, 0);

  return true;
}

size_t AtspiInProcessFuzzer::CustomMutator(uint8_t* data,
                                           size_t size,
                                           size_t max_size,
                                           unsigned int seed) {
  std::minstd_rand random(seed);

  // We almost always want to put in place a valid control path. So at random:
  // 0 = use just libprotobuf-mutator
  // 1 = use libprotobuf-mutator then our mutator
  //     (sometimes this might be useful for instance to get from "panel 2"
  //     to "frame 3", or something. "panel 3" might not be a valid control.)
  // 2-100 = use just our mutator, which will pick a valid control
  int mutation_strategy = std::uniform_int_distribution(0, 100)(random);

  switch (mutation_strategy) {
    case 0:
      return MutateUsingLPM(data, size, max_size, random());
    case 1: {
      size = MutateUsingLPM(data, size, max_size, random());
      std::optional<size_t> new_size =
          MutateControlPath(data, size, max_size, random);
      return new_size.value_or(size);
    }
    default: {
      std::optional<size_t> new_size =
          MutateControlPath(data, size, max_size, random);
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
// We therefore sometimes use this custom mutator to specify controls
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
// CENTIPEDE: In centipede, the mutator and fuzzer run in different
// OS process invocations, so we have to persist the known controls onto disk.
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
  base::ScopedAllowBlockingForTesting allow_blocking;
  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions()
          // centipede may run several fuzzers at once
          .set_exclusive_locking(false),
      sql::test::kTestTag);
  base::FilePath db_path;
  CHECK(base::PathService::Get(base::DIR_TEMP, &db_path));
  db_path = db_path.AppendASCII("atspi_in_process_fuzzer_controls.db");
  CHECK(db_->Open(db_path));
  CHECK(db_->Execute("PRAGMA foreign_keys = ON"));
  // Atomically delete and create tables
  sql::Transaction transaction(db_.get());
  CHECK(transaction.Begin());
  // Delete some tables from older versions of this fuzzer
  DropTableIfExists("roles");
  DropTableIfExists("names");
  DropTableIfExists("prereqs");
  DropTableIfExists("actions");
  DropTableIfExists("controls");
  DropTableIfExists("controlsv2");
  DropTableIfExists("controlsv3");
  // Create the ones we care about nowadays
  if (!db_->DoesTableExist("controlsv4")) {
    CHECK(db_->Execute(
        "create table controlsv4 (id INTEGER PRIMARY KEY, path "
        "TEXT NOT NULL UNIQUE, probably_actionable BOOL, has_prereq BOOL)"));
  }
  if (!db_->DoesTableExist("actionsv2")) {
    CHECK(db_->Execute(
        "create table actionsv2 (id INTEGER PRIMARY KEY, control_id "
        "INTEGER NOT NULL, verb TEXT NOT NULL, FOREIGN KEY(control_id) "
        "REFERENCES controlsv4(id) ON DELETE CASCADE, unique(control_id, "
        "verb))"));
  }
  if (!db_->DoesTableExist("prereqsv2")) {
    CHECK(db_->Execute(
        "create table prereqsv2 (control_id INTEGER NOT NULL, "
        "action_id INTEGER NOT NULL, FOREIGN KEY(control_id) REFERENCES "
        "controlsv4(id) ON DELETE CASCADE,  FOREIGN KEY(action_id) REFERENCES "
        "actionsv2(id) ON DELETE CASCADE, unique(control_id, action_id))"));
  }
  CHECK(transaction.Commit());
}

void Database::DropTableIfExists(const std::string& table_name) {
  if (db_->DoesTableExist(table_name)) {
    CHECK(db_->Execute(base::StrCat({"drop table ", table_name})));
  }
}

std::optional<int64_t> Database::InsertControlPath(const std::string& path,
                                                   bool probably_actionable,
                                                   bool has_prereq) {
  sql::Statement stmt(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO controlsv4 (path, "
      "probably_actionable, has_prereq) VALUES (?, ?, ?)"));
  stmt.BindString(0, path);
  stmt.BindBool(1, probably_actionable);
  // Storing the following bool in the database seems wasteful as it could be calculated
  // during the SELECT statement by joining to the prereqs table. Unfortunately that
  // turns out to be too slow, so we'll store a bool instead.
  stmt.BindBool(2, has_prereq);
  if (!stmt.Run()) {
    return std::nullopt;
  }  // ignore result in case other instances of the fuzzer have the database
     // locked
  sql::Statement get_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "select id from controlsv4 where path = ?"));
  get_statement.BindString(0, path);
  if (!get_statement.Step()) {
    return std::nullopt;
  }
  int64_t controlv2_id = get_statement.ColumnInt64(0);
  return controlv2_id;
}

void Database::InsertControlPathAndPrerequisites(
    const std::string& newly_visible_control,
    const std::optional<ActionPath>& prerequisite_action,
    bool probably_actionable) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::optional<int64_t> control_id =
      InsertControlPath(newly_visible_control, probably_actionable,
                        prerequisite_action.has_value());
  if (!control_id) {
    return;
  }

  if (prerequisite_action) {
    // Almost certainly retrieving an exiting ID
    std::optional<int64_t> prereq_control_id =
        InsertControlPath(prerequisite_action->control_path, true, false);
    if (!prereq_control_id) {
      return;
    }
    sql::Statement stmt(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR IGNORE INTO actionsv2 (control_id, verb) VALUES (?, ?)"));
    stmt.BindInt64(0, *prereq_control_id);
    stmt.BindString(1, prerequisite_action->verb_string);
    if (!stmt.Run()) {
      return;
    }

    sql::Statement get_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "select id from actionsv2 where control_id = ? and verb = ?"));
    get_statement.BindInt64(0, *prereq_control_id);
    get_statement.BindString(1, prerequisite_action->verb_string);
    if (!get_statement.Step()) {
      return;
    }
    int64_t action_id = get_statement.ColumnInt64(0);

    sql::Statement stmt2(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "INSERT OR IGNORE INTO prereqsv2 (control_id, "
                                "action_id) VALUES (?, ?)"));
    stmt2.BindInt64(0, *control_id);
    stmt2.BindInt64(1, action_id);
    if (!stmt2.Run()) {
      return;
    }
  }

  const int64_t kMaxRowsAllowed = 1000;
  // Delete random rows to keep to that maximum size
  sql::Statement delete_stmt(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE from controlsv4 where id in (select id from controlsv4 "
      "order by "
      "random() limit max(0, ((select count(*) from controlsv4) - ?)))"));
  delete_stmt.BindInt64(0, kMaxRowsAllowed);
  base::IgnoreResult(delete_stmt.Run());
}

std::optional<std::string> Database::GetRandomControlPath(
    std::minstd_rand& random) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  size_t random_selector =
      std::uniform_int_distribution<int64_t>(0, INT64_MAX)(random);
  // Complex SQL here - explanation follows.
  // The idea is essentially just to select any 'path' from the 'controlsv4'
  // table which has been filled in with the paths to controls that were
  // previously discovered to actually exist.
  // However, we apply a bias towards some controls, based on these factors:
  // * Whether the control is deemed to be actionable (see
  //   UiNode::ProbablyActionable)
  // * Whether there's a pre-requisite action to make the control appear
  // * The length of the path. We want to poke at deeper more obscure controls.
  // Stepping through the SQL to explain how we do that.
  const std::string query =
      // First let's create a CTE which adds scores for these individual
      // factors.
      "WITH controlsv4_scored AS (select controlsv4.*, "
      "CASE WHEN probably_actionable = true THEN 10 ELSE 1 END AS "
      "actionability_bias, "
      "CASE WHEN has_prereq = true THEN 20 ELSE 1 END AS "
      "prereq_bias, "
      "MIN(LENGTH(path) / 20, 1) AS path_bias "
      "FROM controlsv4), "
      // Now let's create a further CTE which multiplies those biases into one
      // score.
      "controlsv4_with_bias AS ("
      "SELECT id, path, CAST(path_bias * actionability_bias * prereq_bias AS "
      "INTEGER) AS bias FROM controlsv4_scored), "
      // Now create a recursive CTE which is essentially controlsv4 but with
      // rows *REPEATED* based on the bias. This is the clever bit!
      "controlsv4_repeated AS (SELECT id, path, bias, 1 AS counter FROM "
      "controlsv4_with_bias "
      "UNION ALL select id, path, bias, counter + 1 FROM controlsv4_repeated "
      "WHERE counter < bias) "
      // Finally, select a random row from that.
      "SELECT path FROM controlsv4_repeated LIMIT 1 offset (? % (SELECT "
      "count(*) FROM controlsv4_repeated))";
  sql::Statement get_statement(db_->GetCachedStatement(SQL_FROM_HERE, query));
  get_statement.BindInt64(0, random_selector);
  if (!get_statement.Step()) {
    return std::nullopt;
  }
  return get_statement.ColumnString(0);
}

std::optional<ActionPath> Database::GetPrerequisite(const std::string& control,
                                                    std::minstd_rand& random) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  size_t random_selector =
      std::uniform_int_distribution<int64_t>(INT64_MIN, INT64_MAX)(random);
  sql::Statement get_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "with prereq_options as (select prereq_control.path as prereq_path, "
      "this_control.path as this_path, verb from "
      "controlsv4 as this_control, controlsv4 as prereq_control, actionsv2, "
      "prereqsv2 where this_control.id = prereqsv2.control_id and "
      "prereqsv2.action_id "
      "= actionsv2.id and actionsv2.control_id = prereq_control.id)"
      "select prereq_path, verb from prereq_options where this_path = ? limit "
      "1 offset (? % (select count(*) from "
      "prereq_options))"));
  get_statement.BindString(0, control);
  get_statement.BindInt64(1, random_selector);
  if (!get_statement.Step()) {
    // No known pre-requisites to making this control visible, hopefully
    // it's visible in Chrome's freshly-launched UI
    return std::nullopt;
  }
  ActionPath result;
  result.control_path = get_statement.ColumnString(0);
  result.verb_string = get_statement.ColumnString(1);
  return result;
}
