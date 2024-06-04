// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/atspi_in_process_fuzzer.pb.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/base/glib/scoped_gobject.h"

// Controls (by name) which we shouldn't choose.
constexpr auto kBlockedControls =
    base::MakeFixedFlatSet<std::string_view>({"Close"});

using ScopedAtspiAccessible = ScopedGObject<AtspiAccessible>;

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
// The main problem with this fuzzer is that it identifies a path to a control
// based solely on ordinals, so as the Chromium UI evolves, test cases won't
// be stable. It would be better to identify the path to the controls via
// their names; however:
// a) many controls do not have names (though there are other textual
//    identifiers, e.g. class and role, which we could use)
// b) it's believed that libprotobuf-mutator currently is not smart enough
//    to recognize string compares, so wouldn't adequately explore the space
//    of controls without a huge seed corpus or dictionary including every
//    control name.
// If the latter is fixed, we should change the proto here to specify the
// child control based on strings instead of ordinal integers.
class AtspiInProcessFuzzer
    : public InProcessProtoFuzzer<test::fuzzing::atspi_fuzzing::FuzzCase> {
 public:
  AtspiInProcessFuzzer();
  void SetUpOnMainThread() override;

  int Fuzz(const test::fuzzing::atspi_fuzzing::FuzzCase& fuzz_case) override;

  static void MutateControlPath(test::fuzzing::atspi_fuzzing::Action* message,
                                unsigned int seed);

 private:
  void LoadAPage();
  static ScopedAtspiAccessible GetRootNode();
  static std::vector<ScopedAtspiAccessible> GetChildren(
      ScopedAtspiAccessible& node);
  static std::string GetNodeName(ScopedAtspiAccessible& node);
  static bool InvokeAction(ScopedAtspiAccessible& node, size_t action_id);
  static bool ReplaceText(ScopedAtspiAccessible& node,
                          const std::string& newtext);
  static bool SetSelection(ScopedAtspiAccessible& node,
                           const std::vector<uint32_t>& new_selection);
  static bool CheckOk(gboolean ok, GError** error);
};

REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER(AtspiInProcessFuzzer)

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
      "<html><head><title>Test page</title></head><body><form>Username: <input "
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
  for (const test::fuzzing::atspi_fuzzing::Action& action :
       fuzz_case.action()) {
    if (action.path_to_control_size() < 2) {
      // The first couple of levels deep in the accessibility tree are things
      // like the application itself, which are not really interactive.
      // The libfuzzer mutator seems to bias to producing small test cases
      // which want to explore just those nodes. Shortcut things a bit by
      // skipping those without pointlessly poking at the controls.
      return -1;
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
    // We're nowhere near that, and we'd only want to consider doing anything
    // along those lines if this fuzzer finds lots of bugs.
    // Enumerate available controls after each action we take - obviously,
    // clicking on one button may make more buttons available
    ScopedAtspiAccessible current_control = GetRootNode();
    // Keep a record of the control path so we can cache children for later
    // exploration
    std::vector<uint32_t> current_control_path;
    std::vector<ScopedAtspiAccessible> children = GetChildren(current_control);
    for (const uint32_t& ordinal_number : action.path_to_control()) {
      if (children.empty()) {
        return 0;
      }
      // The % here means that these fuzz cases are unstable across versions
      // if the total number of controls at any position in the tree changes,
      // as well as if the specific ordinal of a given control changes. That's
      // a shame, but easiest for now. See comment above about how we might
      // improve things.
      size_t child_ordinal = ordinal_number % children.size();
      current_control = children[child_ordinal];
      current_control_path.push_back(child_ordinal);
      children = GetChildren(current_control);
    }

    std::string control_name = GetNodeName(current_control);
    if (kBlockedControls.contains(control_name)) {
      return -1;  // don't explore this case further
    }

    switch (action.action_choice_case()) {
      case test::fuzzing::atspi_fuzzing::Action::kTakeAction:
        if (!InvokeAction(current_control, action.take_action().action_id())) {
          return 0;  // didn't work this time, but could conceivably work in
                     // future, so don't reject it from the corpus
        }
        break;
      case test::fuzzing::atspi_fuzzing::Action::kReplaceText:
        if (!ReplaceText(current_control, action.replace_text().new_text())) {
          return 0;  // didn't work this time, but could conceivably work in
                     // future, so don't reject it from the corpus
        }
        break;
      case test::fuzzing::atspi_fuzzing::Action::kSetSelection: {
        std::vector<uint32_t> new_selection(
            action.set_selection().selected_child().begin(),
            action.set_selection().selected_child().end());
        if (!SetSelection(current_control, new_selection)) {
          return 0;  // didn't work this time, but could conceivably work in
                     // future, so don't reject it from the corpus
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

std::string AtspiInProcessFuzzer::GetNodeName(ScopedAtspiAccessible& node) {
  std::string retval;
  GError* error = nullptr;

  char* name = atspi_accessible_get_name(node, &error);
  if (!error) {
    retval = name;
  }
  g_clear_error(&error);
  free(name);
  return retval;
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
