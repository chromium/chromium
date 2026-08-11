// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test_internal.h"

#include <compare>
#include <functional>
#include <memory>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "ui/aura/window.h"
#endif

namespace internal {

DEFINE_SAFE_CAST_TARGET(InteractiveBrowserTestPrivate)

// static
const std::string_view InteractiveBrowserTestPrivate::kDumpElementsScript =
    R"(
  function gatherHtmlContent(node, active, params) {
    const result = {
      text: '',
      children: [],
    };
    if (params.count !== undefined && params.count <= 0) {
      return null;
    }
    let hidden = false;
    if (node instanceof ShadowRoot) {
      result.text = '(shadow root)';
      active = node.activeElement;
    } else if (node instanceof Element) {
      if (active === node && !node.shadowRoot) {
        result.text += '[FOCUSED] ';
      }
      if (node.id) {
        result.text += '#' + node.id + ' ';
      }
      result.text += node.tagName.toLowerCase();
      const rect = node.getBoundingClientRect();
      hidden = rect.width <= 0 || rect.height <= 0;
      if (hidden) {
        result.text += ' (not visible)';
      } else {
        const round = (n) => Math.round(n * 10) / 10;
        // x:86-120 y:56-90 (34x34)
        result.text +=
            ' at x:' + round(rect.x) + '-' + round(rect.x + rect.width);
        result.text +=
            ' y:' + round(rect.y) + '-' + round(rect.y + rect.height);
        result.text +=
            ' (' + round(rect.width) + 'x' + round(rect.height) + ')';
      }
    } else {
      return null;
    }

    if (params.count !== undefined) {
      --params.count;
      if (params.count <= 0) {
        result.text += ' --- node limit reached ---';
        return result;
      }
    }

    if (!hidden) {
      if (params.depth === undefined || params.depth > 0) {
        if (params.depth !== undefined) {
          --params.depth;
        }
        for (const child of node.childNodes) {
          const childData = gatherHtmlContent(child, active, params);
          if (childData) {
            result.children.push(childData);
          }
        }
        if (node instanceof Element && node.shadowRoot) {
          const childData = gatherHtmlContent(node.shadowRoot, null, params);
          if (childData) {
            result.children.push(childData);
          }
        }
        if (params.depth !== undefined) {
          ++params.depth;
        }
      } else {
        result.children.push(
            { text: ' --- depth limit reached --- ', children: [] });
      }
    }
    return result;
  }
  function stringifyHtmlContent(node, prefix, last) {
    let text = prefix;
    if (!prefix) {
      prefix += '  ';
    } else {
      if (last) {
        text += '╰─';
        prefix += '   ';
      } else {
        text += '├─';
        prefix += '│  ';
      }
    }
    text += node.text + '\n';
    for (let i = 0; i < node.children.length; ++i) {
      const last_child = (i == node.children.length - 1);
      text += stringifyHtmlContent(node.children[i], prefix, last_child);
    }
    return text;
  }
  function dumpHtmlContent(node, active, params) {
    return stringifyHtmlContent(
        gatherHtmlContent(node, active, params),
        '', false);
  }
)";

InteractiveBrowserTestPrivate::InteractiveBrowserTestPrivate(
    ui::test::internal::InteractiveTestPrivate& test_impl)
    : ui::test::internal::InteractiveTestPrivateFrameworkBase(test_impl) {
  InteractionTestUtilBrowser::PopulateSimulators(test_impl.test_util());
}

InteractiveBrowserTestPrivate::~InteractiveBrowserTestPrivate() = default;

void InteractiveBrowserTestPrivate::DoTestTearDown() {
  // Release any remaining instrumented WebContents.
  instrumented_web_contents_.clear();

  // If the test has elected to engage WebUI code coverage, write out the
  // resulting data.
  if (coverage_observer_) {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name =
        base::StrCat({test_info->test_suite_name(), ".", test_info->name()});
    // Parameterized tests tend to have slashes in them, which can interfere
    // with file system paths. Change them to something else.
    std::replace(test_name.begin(), test_name.end(), '/', '_');

    LOG(INFO) << "Writing out WebUI code coverage data. If this causes the "
                 "test to time out (b/273290598), you may want to disable "
                 "coverage until  the performance can be improved. If the "
                 "test crashes, a  page touched by the test is likely still "
                 "incompatible with coverage (see b/273545898).";

    coverage_observer_->CollectCoverage(test_name);
  }
}

void InteractiveBrowserTestPrivate::MaybeStartWebUICodeCoverage() {
  if (coverage_observer_) {
    return;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    coverage_observer_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir);

    LOG(WARNING) << "Starting WebUI code coverage. This may cause the test to "
                    "take longer, possibly resulting in timeouts. Also, due to "
                    "issues with the coverage logic, some WebUI pages may not "
                    "be compatible with WebUI code coverage.";
  }
}

void InteractiveBrowserTestPrivate::AddInstrumentedWebContents(
    std::unique_ptr<WebContentsInteractionTestUtil> instrumented_web_contents) {
  for (const auto& existing : instrumented_web_contents_) {
    CHECK_NE(instrumented_web_contents->page_identifier(),
             existing->page_identifier());
  }
  instrumented_web_contents_.emplace_back(std::move(instrumented_web_contents));
}

bool InteractiveBrowserTestPrivate::IsInstrumentedWebContents(
    ui::ElementIdentifier element_id) const {
  for (const auto& existing : instrumented_web_contents_) {
    if (existing->page_identifier() == element_id) {
      return true;
    }
  }
  return false;
}

bool InteractiveBrowserTestPrivate::UninstrumentWebContents(
    ui::ElementIdentifier to_remove) {
  for (auto it = instrumented_web_contents_.begin();
       it != instrumented_web_contents_.end(); ++it) {
    if ((*it)->page_identifier() == to_remove) {
      instrumented_web_contents_.erase(it);
      return true;
    }
  }
  return false;
}

std::string InteractiveBrowserTestPrivate::DeepQueryToString(
    const WebContentsInteractionTestUtil::DeepQuery& deep_query) {
  std::ostringstream oss;
  oss << "{";
  for (size_t i = 0; i < deep_query.size(); ++i) {
    if (i) {
      oss << ", ";
    }
    oss << "\"" << deep_query[i] << "\"";
  }
  oss << "}";
  return oss.str();
}

std::string InteractiveBrowserTestPrivate::MakeDumpParams() const {
  std::ostringstream oss;
  oss << "{";
  if (max_dom_nodes_) {
    oss << " count: " << *max_dom_nodes_ << ",";
  }
  if (max_dom_depth_) {
    oss << " depth: " << *max_dom_depth_ << ",";
  }
  oss << " }";
  return oss.str();
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromElement(
    const ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();

  // For instrumented WebContents, we can get the native window directly from
  // the contents object.
  if (el->IsA<TrackedElementWebContents>()) {
    auto* const util = el->AsA<TrackedElementWebContents>()->owner();
    window = util->web_contents()->GetTopLevelNativeWindow();
  }

  return window;
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromContext(
    ui::ElementContext context) const {
  // Use the top-level browser window for the context (assuming there is one).
  if (auto* const browser =
          InteractionTestUtilBrowser::GetBrowserFromContext(context)) {
    if (BrowserView* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser)) {
      return browser_view->GetNativeWindow();
    }
  }
  return gfx::NativeWindow();
}

std::string InteractiveBrowserTestPrivate::DebugDescribeContext(
    ui::ElementContext context) const {
  if (const auto* browser =
          InteractionTestUtilBrowser::GetBrowserFromContext(context)) {
    std::string type;
    switch (browser->GetType()) {
      case Browser::TYPE_APP:
        type = "App window";
        break;
      case Browser::TYPE_APP_POPUP:
        type = "Popup app window";
        break;
      case Browser::TYPE_NORMAL:
        type = "Tabbed browser window";
        break;
      case Browser::TYPE_DEVTOOLS:
        type = "Devtools window";
        break;
      case Browser::TYPE_PICTURE_IN_PICTURE:
        type = "Picture-in-picture window";
        break;
      default:
        type = "Other browser window";
        break;
    }
    type += base::StringPrintf(", %d tab(s) (active: %d)",
                               browser->GetTabStripModel()->count(),
                               browser->GetTabStripModel()->active_index());
    return base::StringPrintf(
        "%s%s profile %s%s at %s",
        (browser->GetWindow()->IsActive() ? "[ACTIVE] " : ""), type,
        browser->GetProfile()->GetDebugName(),
        (browser->GetProfile()->IsOffTheRecord() ? " (off-the-record)" : ""),
        DebugDumpBounds(browser->GetWindow()->GetBounds()));
  }

  return std::string();
}

namespace {

// Converts `dump_info` to debug tree nodes and adds it as a child of `node`.
void AddWebDumpNodes(InteractiveBrowserTestPrivate::DebugTreeNode& node,
                     const base::Value& dump_info) {
  if (!dump_info.is_dict()) {
    LOG(ERROR) << "Expected dict type but got "
               << base::Value::GetTypeName(dump_info.type());
    return;
  }
  const auto& dict = dump_info.GetDict();
  const auto* const text = dict.FindString("text");
  if (!text) {
    LOG(ERROR)
        << "Expected dict to have 'text' field but did not or was wrong type.";
    return;
  }
  auto& new_node = node.children.emplace_back(*text);
  const auto* const children = dict.FindList("children");
  if (children) {
    for (const auto& child : *children) {
      AddWebDumpNodes(new_node, child);
    }
  }
}

}  // namespace

std::vector<InteractiveBrowserTestPrivate::DebugTreeNode>
InteractiveBrowserTestPrivate::DebugDumpElements(
    std::set<const ui::TrackedElement*>& elements) const {
  std::vector<InteractiveBrowserTestPrivate::DebugTreeNode> nodes;
  for (auto it = elements.begin(); it != elements.end();) {
    auto* const el = *it;
    if (const auto* contents = el->AsA<TrackedElementWebContents>()) {
      auto* const web_contents = contents->owner()->web_contents();
      int index = TabStripModel::kNoTab;
      if (const auto* browser =
              InteractionTestUtilBrowser::GetBrowserFromContext(
                  el->context())) {
        index =
            browser->GetTabStripModel()->GetIndexOfWebContents(web_contents);
      }
      auto& new_node = nodes.emplace_back(base::StringPrintf(
          "WebContents %s - %s at %s with URL \"%s\"",
          (index == TabStripModel::kNoTab
               ? "in secondary UI"
               : base::StringPrintf("in tab %d", index).c_str()),
          el->identifier().GetName(), DebugDumpBounds(el->GetScreenBounds()),
          web_contents->GetURL().spec().c_str()));
      it = elements.erase(it);
      if (auto* const util =
              const_cast<WebContentsInteractionTestUtil*>(contents->owner());
          util && util->is_page_loaded()) {
        std::string error_message;
        const auto value =
            util->Evaluate(base::StringPrintf(
                               R"(function() {
              %s;
              return gatherHtmlContent(
                  document.body, document.activeElement, %s);
            })",
                               kDumpElementsScript, MakeDumpParams()),
                           &error_message);
        if (!error_message.empty()) {
          LOG(ERROR) << "Unable to retrieve contents of " << *contents << ": "
                     << error_message;
        } else {
          new_node.text +=
              " (note that descendant bounds are relative to this element)";
          AddWebDumpNodes(new_node, value);
        }
      }
    } else {
      ++it;
    }
  }
  return nodes;
}

MatchableValue::MatchableValue() noexcept = default;
MatchableValue::MatchableValue(const base::Value& value) noexcept
    : value_(value.Clone()) {}
MatchableValue::MatchableValue(base::Value&& value) noexcept
    : value_(std::move(value)) {}
MatchableValue::MatchableValue(const MatchableValue& value) noexcept
    : value_(value.value_.Clone()) {}
MatchableValue::MatchableValue(MatchableValue&&) noexcept = default;
MatchableValue& MatchableValue::operator=(const base::Value& value) noexcept {
  value_ = value.Clone();
  return *this;
}
MatchableValue& MatchableValue::operator=(base::Value&& value) noexcept {
  value_ = std::move(value);
  return *this;
}
MatchableValue& MatchableValue::operator=(
    const MatchableValue& value) noexcept {
  if (this != &value) {
    value_ = value.value_.Clone();
  }
  return *this;
}
MatchableValue& MatchableValue::operator=(MatchableValue&&) noexcept = default;
MatchableValue::~MatchableValue() = default;

void CheckValueTypes(const MatchableValue& source,
                     const MatchableValue& target) {
  using Type = base::Value::Type;
  const auto source_type = source.value().type();
  const auto target_type = target.value().type();

  if (target_type == Type::DOUBLE &&
      (source_type == Type::DOUBLE || source_type == Type::INTEGER)) {
    // This is an allowed conversion.
    return;
  }

  // Explicitly don't allow downcast to integer for comparison.
  if (target_type == Type::INTEGER) {
    CHECK_NE(source_type, Type::DOUBLE)
        << "JS returned a floating-point value (" << source
        << ") but comparison was with an integer (" << target
        << "). If there is any chance the value will be floating-point, "
           "compare to a double value instead.";
  }

  // Otherwise, the types *must* match.
  CHECK_EQ(source_type, target_type) << "Type mismatch attempting to compare "
                                     << source << " (from JS) and " << target;
}

bool MatchableValue::operator==(const MatchableValue& other) const {
  CheckValueTypes(*this, other);
  if (other.value_.type() == base::Value::Type::DOUBLE) {
    return value_.GetDouble() == other.value_.GetDouble();
  }
  return value_ == other.value_;
}

namespace {

template <template <typename...> class Op>
bool MatchableValueCompare(const MatchableValue& lhs,
                           const MatchableValue& rhs) {
  CheckValueTypes(lhs, rhs);
  switch (rhs.value().type()) {
    case base::Value::Type::DOUBLE:
      return Op<double>()(lhs.value().GetDouble(), rhs.value().GetDouble());
    case base::Value::Type::INTEGER:
      return Op<double>()(lhs.value().GetInt(), rhs.value().GetInt());
    case base::Value::Type::STRING:
      return Op<std::string>()(lhs.value().GetString(),
                               rhs.value().GetString());
    default:
      NOTREACHED() << "Target value " << rhs << " (" << rhs.value().type()
                   << ") does not support greater than/less than comparison.";
  }
}

}  // namespace

MatchableValue::operator std::string() const {
  return value_.GetString();
}

bool MatchableValue::operator<(const MatchableValue& other) const {
  return MatchableValueCompare<std::less>(*this, other);
}

bool MatchableValue::operator>(const MatchableValue& other) const {
  return MatchableValueCompare<std::greater>(*this, other);
}

bool MatchableValue::operator<=(const MatchableValue& other) const {
  return MatchableValueCompare<std::less_equal>(*this, other);
}

bool MatchableValue::operator>=(const MatchableValue& other) const {
  return MatchableValueCompare<std::greater_equal>(*this, other);
}

std::ostream& operator<<(std::ostream& out, const MatchableValue& value) {
  return out << value.value();
}

bool IsTruthyMatcher::MatchAndExplain(
    const internal::MatchableValue& x,
    testing::MatchResultListener* listener) const {
  return WebContentsInteractionTestUtil::IsTruthy(x.value());
}

void IsTruthyMatcher::DescribeTo(std::ostream* os) const {
  *os << "is truthy";
}

void IsTruthyMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "is falsy";
}

}  // namespace internal
