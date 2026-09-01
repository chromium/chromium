// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"

#include <string>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace page_actions {

namespace {

// JavaScript function for traversing shadow DOM trees to find an element
// matching the predicate.
constexpr char kFindDeepJS[] = R"(
  function findDeep(root, predicate) {
    if (!root) return null;
    if (predicate(root)) return root;
    const children = root.shadowRoot
        ? Array.from(root.shadowRoot.querySelectorAll('*'))
        : Array.from(root.querySelectorAll('*'));
    for (const child of children) {
      if (predicate(child)) return child;
      if (child.shadowRoot) {
        const found = findDeep(child, predicate);
        if (found) return found;
      }
    }
    return null;
  }
)";

}  // namespace

PageActionTestAccessor::PageActionTestAccessor(BrowserWindowInterface* browser,
                                               actions::ActionId action_id)
    : browser_(browser), action_id_(action_id) {}

PageActionTestAccessor::~PageActionTestAccessor() = default;

page_actions::PageActionViewInterface* PageActionTestAccessor::GetInterface() {
  if (!browser_) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  return browser_view->toolbar_button_provider()->GetPageActionViewInterface(
      action_id_);
}

page_actions::WebUIPageActionControl*
PageActionTestAccessor::GetWebUIPageActionControl() {
  if (!browser_) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  if (features::IsWebUILocationBarEnabled()) {
    if (auto* const webui_view = browser_view->toolbar_button_provider()
                                     ->GetWebUIToolbarViewForTesting()) {
      if (auto* const loc_bar = webui_view->GetLocationBar()) {
        return &loc_bar->page_action_control();
      }
    }
  }
  return nullptr;
}

const page_actions::PageActionModelInterface*
PageActionTestAccessor::GetModel() {
  if (auto* const control = GetWebUIPageActionControl()) {
    return control->GetObservedModel(action_id_);
  }
  return nullptr;
}

page_actions::PageActionView* PageActionTestAccessor::GetPageActionView() {
  if (features::IsWebUILocationBarEnabled()) {
    return nullptr;
  }
  auto* interface_ptr = GetInterface();
  if (!interface_ptr) {
    return nullptr;
  }
  return static_cast<page_actions::PageActionView*>(interface_ptr);
}

ui::TrackedElementWebUI* PageActionTestAccessor::GetTrackedElement() {
  if (!browser_) {
    return nullptr;
  }
  PageActionPropertiesProvider provider;
  if (!provider.Contains(action_id_)) {
    return nullptr;
  }
  ui::ElementIdentifier element_id =
      provider.GetProperties(action_id_).element_identifier;
  if (!element_id) {
    return nullptr;
  }
  BrowserElements* browser_elements = BrowserElements::From(browser_);
  if (!browser_elements) {
    return nullptr;
  }
  ui::TrackedElement* element = browser_elements->GetElement(element_id);
  if (!element) {
    return nullptr;
  }
  return element->AsA<ui::TrackedElementWebUI>();
}

views::View* PageActionTestAccessor::GetView() {
  return GetPageActionView();
}

content::WebContents* PageActionTestAccessor::GetWebContents() {
  if (!browser_) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  WebUIToolbarWebView* webui_view =
      browser_view->toolbar_button_provider()->GetWebUIToolbarViewForTesting();
  if (!webui_view || !webui_view->GetWebViewForTesting()) {
    return nullptr;
  }
  return webui_view->GetWebViewForTesting()->web_contents();
}

bool PageActionTestAccessor::EvaluateWebUI(
    std::string_view element_predicate_js) {
  if (auto* tracked_el = GetTrackedElement()) {
    if (content::WebContents* contents = GetWebContents()) {
      const std::string script = base::StringPrintf(
          R"((() => {
            const manager = window._trackedElementManager;
            if (!manager) return false;
            const tracked = manager.getElementWithId({
              nativeIdentifier: "%s",
              secondaryIdentifier: "%s"
            });
            if (!tracked || !tracked.element) return false;
            const el = tracked.element;
            return (%s)(el);
          })())",
          tracked_el->identifier().GetName().c_str(),
          tracked_el->GetSecondaryIdentifier().c_str(),
          std::string(element_predicate_js).c_str());
      content::EvalJsResult result = content::EvalJs(contents, script);
      if (result.is_bool()) {
        return result.ExtractBool();
      }
    }
  }
  // TODO(crbug.com/526715177): Remove this fallback once support for
  // TrackedElements in page actions is implemented.
  content::WebContents* contents = GetWebContents();
  if (contents) {
    const int action_id_int = static_cast<int>(
        webui_toolbar::ActionIdToMojomPageActionId(action_id_));
    const std::string script = base::StringPrintf(
        R"((() => {
          %s
          const el = findDeep(document.body,
                              e => e.state?.pageActionId === %d);
          if (!el) return false;
          return (%s)(el);
        })())",
        kFindDeepJS, action_id_int, std::string(element_predicate_js).c_str());
    content::EvalJsResult result = content::EvalJs(contents, script);
    if (result.is_bool()) {
      return result.ExtractBool();
    }
  }
  return false;
}

bool PageActionTestAccessor::GetVisible() {
  if (features::IsWebUILocationBarEnabled()) {
    if (const auto* model = GetModel()) {
      return model->GetVisible();
    }
    return EvaluateWebUI(
        "(el) => !el.hidden && window.getComputedStyle(el).display !== 'none'");
  }
  auto* pav = GetPageActionView();
  return pav ? pav->GetVisible() : false;
}

bool PageActionTestAccessor::IsChipVisible() {
  if (features::IsWebUILocationBarEnabled()) {
    if (const auto* model = GetModel()) {
      return model->GetVisible() && model->IsChipShowing();
    }
    return EvaluateWebUI(
        R"((el) => {
          if (el.hidden || window.getComputedStyle(el).display === 'none') {
            return false;
          }
          return !!el.state?.shouldShowChip && !!el.state?.text;
        })");
  }
  auto* pav = GetPageActionView();
  if (!pav || !pav->GetVisible()) {
    return false;
  }
  return pav->IsChipVisible();
}

bool PageActionTestAccessor::IsIconVisible() {
  if (features::IsWebUILocationBarEnabled()) {
    if (const auto* model = GetModel()) {
      return model->GetVisible() && !model->IsChipShowing();
    }
    return EvaluateWebUI(
        R"((el) => {
          if (el.hidden || window.getComputedStyle(el).display === 'none') {
            return false;
          }
          return !el.state?.shouldShowChip || !el.state?.text;
        })");
  }
  auto* pav = GetPageActionView();
  if (!pav || !pav->GetVisible()) {
    return false;
  }
  return !pav->IsChipVisible();
}

bool PageActionTestAccessor::HasFocus() {
  if (auto* pav = GetPageActionView()) {
    return pav->HasFocus();
  }

  return EvaluateWebUI("(el) => el.matches(':focus')");
}

bool PageActionTestAccessor::IsAnimating() {
  if (features::IsWebUILocationBarEnabled()) {
    return EvaluateWebUI(
        R"((el) => {
          const btn = el.shadowRoot
              ? el.shadowRoot.querySelector('button, [role="button"]')
              : null;
          const anims = [
            ...el.getAnimations(),
            ...(btn ? btn.getAnimations() : [])
          ];
          return anims.some(a => a.playState === 'running');
        })");
  }
  auto* pav = GetPageActionView();
  if (!pav) {
    return false;
  }
  return pav->is_animating_label();
}

std::u16string PageActionTestAccessor::GetText() {
  if (!IsChipVisible()) {
    return std::u16string();
  }
  if (features::IsWebUILocationBarEnabled()) {
    if (const auto* model = GetModel()) {
      return model->GetText();
    }
    if (auto* tracked_el = GetTrackedElement()) {
      if (content::WebContents* contents = GetWebContents()) {
        const std::string script = base::StringPrintf(
            R"((() => {
              const manager = window._trackedElementManager;
              if (!manager) return '';
              const tracked = manager.getElementWithId({
                nativeIdentifier: "%s",
                secondaryIdentifier: "%s"
              });
              if (!tracked || !tracked.element) return '';
              return (tracked.element.state?.text || '').toString();
            })())",
            tracked_el->identifier().GetName().c_str(),
            tracked_el->GetSecondaryIdentifier().c_str());
        content::EvalJsResult result = content::EvalJs(contents, script);
        if (result.is_string()) {
          return base::UTF8ToUTF16(result.ExtractString());
        }
      }
    }
    return std::u16string();
  }
  auto* pav = GetPageActionView();
  return pav ? std::u16string(pav->GetText()) : std::u16string();
}

std::u16string PageActionTestAccessor::GetTooltipText() {
  if (auto* interface_ptr = GetInterface()) {
    return interface_ptr->GetTooltipText();
  }
  if (const auto* model = GetModel()) {
    return model->GetTooltipText();
  }
  return std::u16string();
}

std::u16string PageActionTestAccessor::GetAccessibleName() {
  if (auto* interface_ptr = GetInterface()) {
    return interface_ptr->GetAccessibleName();
  }
  if (const auto* model = GetModel()) {
    return model->GetAccessibleName();
  }
  return std::u16string();
}

ui::ImageModel PageActionTestAccessor::GetImage() {
  if (features::IsWebUILocationBarEnabled()) {
    if (const auto* model = GetModel()) {
      return model->GetImage();
    }
    return ui::ImageModel();
  }
  auto* pav = GetPageActionView();
  return pav ? pav->GetImageModel(views::Button::STATE_NORMAL)
                   .value_or(ui::ImageModel())
             : ui::ImageModel();
}

void PageActionTestAccessor::Click(page_actions::PageActionTrigger trigger) {
  if (features::IsWebUILocationBarEnabled()) {
    if (auto* tracked_el = GetTrackedElement()) {
      if (content::WebContents* contents = GetWebContents()) {
        const std::string script = base::StringPrintf(
            R"((() => {
              const manager = window._trackedElementManager;
              if (!manager) return false;
              const tracked = manager.getElementWithId({
                nativeIdentifier: "%s",
                secondaryIdentifier: "%s"
              });
              if (!tracked || !tracked.element) return false;
              const el = tracked.element;
              const btn = el.shadowRoot
                  ? (el.shadowRoot.querySelector(
                         '#button, toolbar-chip-button, toolbar-button, button, [role="button"]') || el)
                  : el;
              btn.click();
              return true;
            })())",
            tracked_el->identifier().GetName().c_str(),
            tracked_el->GetSecondaryIdentifier().c_str());
        content::EvalJsResult result = content::EvalJs(contents, script);
        if (result.is_bool() && result.ExtractBool()) {
          return;
        }
      }
    }
    if (auto* const control = GetWebUIPageActionControl()) {
      control->OnPageActionClick(
          webui_toolbar::ActionIdToMojomPageActionId(action_id_), trigger,
          base::DoNothing());
      return;
    }
    return;
  }
  if (auto* pav = GetPageActionView()) {
    ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    views::test::ButtonTestApi(pav).NotifyClick(event);
  }
}

void PageActionTestAccessor::SetSuppressionThreshold(
    base::TimeDelta threshold) {
  if (auto* const control = GetWebUIPageActionControl()) {
    control->SetSuppressionThresholdForTesting(threshold);
  }
}

}  // namespace page_actions
