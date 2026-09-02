// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/headless/headless_command_processor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

namespace {

::AvatarToolbarButtonState MapMojomAvatarState(
    toolbar_ui_api::mojom::AvatarToolbarButtonState state) {
  switch (state) {
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kGuestSession:
      return ::AvatarToolbarButtonState::kGuestSession;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kIncognitoProfile:
      return ::AvatarToolbarButtonState::kIncognitoProfile;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::
        kEnterpriseIsolatedProfile:
      return ::AvatarToolbarButtonState::kEnterpriseIsolatedProfile;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kExplicitTextShowing:
      return ::AvatarToolbarButtonState::kExplicitTextShowing;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kOnSignin:
      return ::AvatarToolbarButtonState::kOnSignin;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kShowIdentityName:
      return ::AvatarToolbarButtonState::kShowIdentityName;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kSigninPending:
      return ::AvatarToolbarButtonState::kSigninPending;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kSyncPaused:
      return ::AvatarToolbarButtonState::kSyncPaused;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kUpgradeClientError:
      return ::AvatarToolbarButtonState::kUpgradeClientError;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kPassphraseError:
      return ::AvatarToolbarButtonState::kPassphraseError;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::
        kBookmarksLimitExceeded:
      return ::AvatarToolbarButtonState::kBookmarksLimitExceeded;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kSyncError:
      return ::AvatarToolbarButtonState::kSyncError;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kPasskeysLockedError:
      return ::AvatarToolbarButtonState::kPasskeysLockedError;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kPromo:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      return ::AvatarToolbarButtonState::kPromo;
#else
      NOTREACHED();
#endif
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kManagement:
      return ::AvatarToolbarButtonState::kManagement;
    case toolbar_ui_api::mojom::AvatarToolbarButtonState::kNormal:
      return ::AvatarToolbarButtonState::kNormal;
  }
}

// JavaScript template for locating an extension button in the WebUI toolbar
// and performing an action on it. Expects two format arguments:
// 1. (const char*): The extension ID (or empty string for the puzzle piece
//    extensions menu button) to locate the button element `btn`.
// 2. (const char*): The JavaScript statement(s) to execute on `btn` (e.g.
//    "btn.click();" or dispatching an event).
constexpr char kClickExtensionButtonScript[] = R"(
  (() => {
    const app = document.querySelector('toolbar-app');
    const extensionsContainer = app.shadowRoot.querySelector('#extensions');
    const extensionElements = extensionsContainer.shadowRoot
        .querySelectorAll('webui-toolbar-extension');
    const el = Array.from(extensionElements)
        .find(el => el.state.id === '%s');
    const btn = el.shadowRoot.querySelector('cr-button');
    %s
  })();
)";

}  // namespace

void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser) {
  if (headless::ShouldProcessHeadlessCommands()) {
    return;
  }
  if (!browser || (!features::IsWebUIToolbarEnabled() &&
                   !base::FeatureList::IsEnabled(
                       features::kWebUIToolbarProcessOverheadExperiment))) {
    return;
  }

  base::RunLoop run_loop;
  BrowserElements* browser_elements = BrowserElements::From(browser);
  if (!browser_elements) {
    return;
  }
  ui::TrackedElement* element =
      browser_elements->GetElement(kWebUIToolbarElementIdentifier);
  if (!element) {
    return;
  }
  WebUIToolbarWebView* webui_toolbar = views::AsViewClass<WebUIToolbarWebView>(
      element->AsA<views::TrackedElementViews>()->view());
  if (!webui_toolbar) {
    return;
  }

  webui_toolbar->SetDidFirstNonEmptyPaintCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

void WaitForInitialWebUIToolbar(BrowserWindowInterface* browser) {
  base::RunLoop run_loop;
  InitialWebUIManager* manager = InitialWebUIManager::From(browser);
  if (!manager || !manager->RequestDeferShow(run_loop.QuitClosure())) {
    return;
  }
  run_loop.Run();
}

void SetUpWebUI(const ui::ElementIdentifier& element_id,
                ui::TrackedElement** element_out,
                WebUIToolbarWebView** webui_toolbar_view_out,
                views::WebView** web_view_out,
                BrowserWindowInterface* browser) {
  // Wait for the WebUIToolbarWebView to be available.
  *webui_toolbar_view_out = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (!browser_view || !browser_view->toolbar()) {
      return false;
    }
    ToolbarButtonProvider* provider = browser_view->toolbar();
    *webui_toolbar_view_out = provider->GetWebUIToolbarViewForTesting();
    return *webui_toolbar_view_out != nullptr;
  }));
  ASSERT_TRUE(*webui_toolbar_view_out);

  if (element_id == kWebUIToolbarElementIdentifier) {
    // We already have the view, and the Basic test doesn't strictly need the
    // TrackedElement. ElementTracker might be flaky or slow here.
    *element_out = views::ElementTrackerViews::GetInstance()->GetElementForView(
        *webui_toolbar_view_out);
  } else {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      *element_out = BrowserElements::From(browser)->GetElement(element_id);
      return *element_out != nullptr;
    }));
    ASSERT_TRUE(*element_out);
  }

  ASSERT_EQ((*webui_toolbar_view_out)->children().size(), 1u);
  *web_view_out = views::AsViewClass<views::WebView>(
      (*webui_toolbar_view_out)->children()[0].get());
  ASSERT_TRUE(*web_view_out);

  // Wait for the WebView to finish composition.
  content::WaitForCopyableViewInWebContents((*web_view_out)->GetWebContents());
}

WebUIToolbarWebView* GetWebUIToolbarWebView(BrowserWindowInterface* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetWebUIToolbarViewForTesting();
}

AvatarButtonUpdateWaiter::AvatarButtonUpdateWaiter(
    AvatarToolbarButtonInterface* button)
    : quit_closure_(run_loop_.QuitClosure()) {
  if (button) {
    scoped_observation_.Observe(button);
  }
}

AvatarButtonUpdateWaiter::~AvatarButtonUpdateWaiter() = default;

void AvatarButtonUpdateWaiter::Wait() {
  if (updated_) {
    return;
  }
  run_loop_.Run();
}

void AvatarButtonUpdateWaiter::OnIconUpdated() {
  updated_ = true;
  quit_closure_.Run();
}

AvatarToolbarButtonTestAccessor::AvatarToolbarButtonTestAccessor(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  WaitForAvatarButton();
}

AvatarToolbarButtonTestAccessor::~AvatarToolbarButtonTestAccessor() = default;

void AvatarToolbarButtonTestAccessor::WaitForAvatarButton() {
  ui::ElementContext context;
  if (BrowserElements* browser_elements = BrowserElements::From(browser_)) {
    context = browser_elements->GetContext();
  } else {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (!browser_view) {
      return;
    }
    context = views::ElementTrackerViews::GetContextForView(browser_view);
  }

  if (ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kToolbarAvatarButtonElementId, context)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // The avatar button is only added to normal browsers (those with a tab
  // strip).
  if (!browser_ ||
      !WindowFeatureController::From(browser_)->SupportsWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTabStrip)) {
    return;
  }
#endif

  Profile* const profile = browser_->GetProfile();
  bool show_avatar_toolbar_button = true;
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS only badges Incognito, Guest, and captive portal signin icons in
  // the browser window.
  show_avatar_toolbar_button =
      profile->IsPrimaryOTRProfileWithRegularParent() ||
      profile->IsGuestSession() ||
      (profile->IsOffTheRecord() &&
       profile->GetOTRProfileID().IsCaptivePortal());
#else
  // DevTools profiles are OffTheRecord, so hide it there.
  show_avatar_toolbar_button =
      profile->IsPrimaryOTRProfileWithRegularParent() ||
      profile->IsGuestSession() || profile->IsRegularProfile();
#endif

  if (!show_avatar_toolbar_button) {
    return;
  }

  base::RunLoop run_loop;
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kToolbarAvatarButtonElementId, context,
          base::BindRepeating(
              [](base::RunLoop* run_loop, ui::TrackedElement* element) {
                run_loop->Quit();
              },
              &run_loop));
  run_loop.Run();
}

bool AvatarToolbarButtonTestAccessor::WaitForText(const std::u16string& text) {
  return base::test::RunUntil([this, text]() { return GetText() == text; });
}

bool AvatarToolbarButtonTestAccessor::WaitForTextNotEqual(
    const std::u16string& text) {
  return base::test::RunUntil([this, text]() { return GetText() != text; });
}

bool AvatarToolbarButtonTestAccessor::WaitForState(
    AvatarToolbarButtonState state) {
  return base::test::RunUntil([this, state]() { return GetState() == state; });
}

std::unique_ptr<AvatarButtonUpdateWaiter>
AvatarToolbarButtonTestAccessor::CreateUpdateWaiter() {
  AvatarToolbarButtonInterface* button = GetInterface();
  return std::make_unique<AvatarButtonUpdateWaiter>(button);
}

AvatarToolbarButtonState AvatarToolbarButtonTestAccessor::GetState() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> AvatarToolbarButtonState {
            return button ? button->state_manager_.GetActiveState()
                          : AvatarToolbarButtonState::kNormal;
          },
          [this](WebUIAvatarToolbarButton* button) -> AvatarToolbarButtonState {
            if (!button) {
              return AvatarToolbarButtonState::kNormal;
            }
            if (ShouldUseCppFallback(button)) {
              return button->state_manager_
                         ? button->state_manager_->GetActiveState()
                         : AvatarToolbarButtonState::kNormal;
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return AvatarToolbarButtonState::kNormal;
            }
            int state_val =
                content::EvalJs(
                    contents,
                    "(async () => {"
                    "  const app = document.querySelector('toolbar-app');"
                    "  if (!app) return -1;"
                    "  await app.updateComplete;"
                    "  const btn = "
                    "app.shadowRoot?.querySelector('avatar-button');"
                    "  if (!btn) return -1;"
                    "  await btn.updateComplete;"
                    "  return btn.state?.state !== undefined ? btn.state.state "
                    ": -1;"
                    "})()")
                    .ExtractInt();
            if (state_val == -1) {
              return AvatarToolbarButtonState::kNormal;
            }
            return MapMojomAvatarState(
                static_cast<toolbar_ui_api::mojom::AvatarToolbarButtonState>(
                    state_val));
          },
      },
      GetButton());
}

bool AvatarToolbarButtonTestAccessor::WaitForRenderedTooltipText(
    const std::u16string& text) {
  return base::test::RunUntil(
      [this, text]() { return GetRenderedTooltipText(gfx::Point()) == text; });
}

bool AvatarToolbarButtonTestAccessor::WaitForAccessibilityLabel(
    const std::u16string& text) {
  return base::test::RunUntil(
      [this, text]() { return GetAccessibilityLabel() == text; });
}

bool AvatarToolbarButtonTestAccessor::WaitForAccessibilityDescription(
    const std::u16string& text) {
  return base::test::RunUntil(
      [this, text]() { return GetAccessibilityDescription() == text; });
}

bool AvatarToolbarButtonTestAccessor::WaitForEnabled(bool enabled) {
  return base::test::RunUntil(
      [this, enabled]() { return GetEnabled() == enabled; });
}

AvatarToolbarButtonInterface* AvatarToolbarButtonTestAccessor::GetInterface() {
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return nullptr;
  }
  return browser_view->toolbar_button_provider()
      ->GetAvatarToolbarButtonInterface();
}

content::WebContents* AvatarToolbarButtonTestAccessor::GetWebContents() {
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return nullptr;
  }
  views::View* webui_toolbar =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kWebUIToolbarElementIdentifier,
          views::ElementTrackerViews::GetContextForView(browser_view));
  if (!webui_toolbar) {
    return nullptr;
  }
  return views::AsViewClass<WebUIToolbarWebView>(webui_toolbar)
      ->GetWebViewForTesting()
      ->web_contents();
}

bool AvatarToolbarButtonTestAccessor::ShouldUseCppFallback(
    WebUIAvatarToolbarButton* button) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!button || !button->state_manager_) {
    return false;
  }
  // On ChromeOS, the avatar button is hidden for normal profiles. In C++ Views,
  // the button view still exists and is updated even when hidden, and many
  // tests assert on its state/text. In WebUI, we don't render the element in
  // the DOM at all when hidden. To allow these tests to pass without adding
  // ChromeOS-specific conditions to every test, we fall back to querying the
  // C++ state manager when the button is hidden.
  Profile* profile = button->state_manager_->browser()->GetProfile();
  return !AvatarToolbarButtonInterface::CanShowForProfile(profile);
#else
  // On other platforms, the button is always visible for the profiles used in
  // these tests, so we should always test the actual WebUI DOM.
  return false;
#endif
}

AvatarToolbarButtonTestAccessor::ButtonVariant
AvatarToolbarButtonTestAccessor::GetButton() {
  AvatarToolbarButtonInterface* interface = GetInterface();
  if (!interface) {
    return static_cast<AvatarToolbarButton*>(nullptr);
  }
  if (features::IsWebUIAvatarButtonEnabled()) {
    return static_cast<WebUIAvatarToolbarButton*>(interface);
  }
  return static_cast<AvatarToolbarButton*>(interface);
}

bool AvatarToolbarButtonTestAccessor::GetEnabled() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> bool {
            return button ? button->GetEnabled() : false;
          },
          [this](WebUIAvatarToolbarButton* button) -> bool {
            if (!button) {
              return false;
            }
            if (ShouldUseCppFallback(button)) {
#if BUILDFLAG(IS_CHROMEOS)
              Profile* profile =
                  button->state_manager_->browser()->GetProfile();
              return profile->IsOffTheRecord() && !profile->IsGuestSession() &&
                     !profile->GetOTRProfileID().IsCaptivePortal();
#else
              return true;
#endif
            }
            content::WebContents* contents = GetWebContents();
            return contents &&
                   content::EvalJs(
                       contents,
                       "(async () => {"
                       "  const app = document.querySelector('toolbar-app');"
                       "  if (!app) return false;"
                       "  await app.updateComplete;"
                       "  const btn = "
                       "app.shadowRoot?.querySelector('avatar-button');"
                       "  if (!btn) return false;"
                       "  await btn.updateComplete;"
                       "  const chip = "
                       "btn.shadowRoot?.querySelector('#button');"
                       "  if (!chip) return false;"
                       "  await chip.updateComplete;"
                       "  return !chip.disabled;"
                       "})()")
                       .ExtractBool();
          },
      },
      GetButton());
}

bool AvatarToolbarButtonTestAccessor::GetVisible() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) {
            return button ? button->GetVisible() : false;
          },
          [this](WebUIAvatarToolbarButton* button) {
            if (!button) {
              return false;
            }
            content::WebContents* contents = GetWebContents();
            return contents &&
                   content::EvalJs(
                       contents,
                       "!!document.querySelector('toolbar-app')"
                       "?.shadowRoot?.querySelector('avatar-button')")
                       .ExtractBool();
          },
      },
      GetButton());
}

std::u16string AvatarToolbarButtonTestAccessor::GetText() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> std::u16string {
            return button ? std::u16string(button->GetText())
                          : std::u16string();
          },
          [this](WebUIAvatarToolbarButton* button) -> std::u16string {
            if (!button) {
              return std::u16string();
            }
            if (ShouldUseCppFallback(button)) {
              StateProvider* active_provider =
                  button->state_manager_->GetActiveStateProvider();
              return active_provider->GetText();
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return std::u16string();
            }
            return base::UTF8ToUTF16(
                content::EvalJs(
                    contents,
                    "(async () => {"
                    "  const app = document.querySelector('toolbar-app');"
                    "  if (!app) return '';"
                    "  await app.updateComplete;"
                    "  const btn = "
                    "app.shadowRoot?.querySelector('avatar-button');"
                    "  if (!btn) return '';"
                    "  await btn.updateComplete;"
                    "  const textEl = btn.shadowRoot?.querySelector('#text');"
                    "  return textEl?.textContent?.trim() || '';"
                    "})()")
                    .ExtractString());
          },
      },
      GetButton());
}

void AvatarToolbarButtonTestAccessor::Click() {
  std::visit(
      [](auto* button) {
        if (!button) {
          return;
        }
        using T = std::decay_t<decltype(*button)>;
        if constexpr (std::is_same_v<T, AvatarToolbarButton>) {
          views::test::InteractionTestUtilSimulatorViews::PressButton(
              button, ui::test::InteractionTestUtil::InputType::kMouse);
        } else {
          button->ButtonPressed(/*is_source_accelerator=*/false);
        }
      },
      GetButton());
}

void AvatarToolbarButtonTestAccessor::SetAnnounceCallbackForTesting(
    base::OnceCallback<void(std::u16string)> callback) {
  if (AvatarToolbarButtonInterface* interface = GetInterface()) {
    interface->SetAnnounceCallbackForTesting(std::move(callback));
  }
}

views::Widget* AvatarToolbarButtonTestAccessor::GetWidget() {
  return std::visit(absl::Overload{
                        [](AvatarToolbarButton* button) {
                          return button ? button->GetWidget() : nullptr;
                        },
                        [this](WebUIAvatarToolbarButton* button) {
                          return BrowserView::GetBrowserViewForBrowser(browser_)
                              ->toolbar_button_provider()
                              ->GetWebUIToolbarViewForTesting()
                              ->GetWidget();
                        },
                    },
                    GetButton());
}

gfx::ImageSkia AvatarToolbarButtonTestAccessor::GetImage(
    views::Button::ButtonState state) {
  return std::visit(absl::Overload{
                        [state](AvatarToolbarButton* button) {
                          return button ? button->GetImage(state)
                                        : gfx::ImageSkia();
                        },
                        [](WebUIAvatarToolbarButton* button) {
                          NOTIMPLEMENTED();
                          return gfx::ImageSkia();
                        },
                    },
                    GetButton());
}

std::string AvatarToolbarButtonTestAccessor::GetImageUrl() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> std::string {
            return std::string();
          },
          [this](WebUIAvatarToolbarButton* button) -> std::string {
            if (!button) {
              return std::string();
            }
            if (ShouldUseCppFallback(button)) {
              if (button->avatar_icon_handle_.is_null()) {
                return std::string();
              }
              auto updates = button->delegate_->GetIconTable().GetFullState();
              for (const auto& update : updates) {
                if (update->handle_id ==
                    button->avatar_icon_handle_.HandleId().value()) {
                  return update->icon_url_or_name.value_or(std::string());
                }
              }
              return std::string();
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return std::string();
            }
            return content::EvalJs(
                       contents,
                       "(async () => {"
                       "  const app = document.querySelector('toolbar-app');"
                       "  if (!app) return '';"
                       "  await app.updateComplete;"
                       "  const btn = "
                       "app.shadowRoot?.querySelector('avatar-button');"
                       "  if (!btn) return '';"
                       "  await btn.updateComplete;"
                       "  const iconEl = "
                       "btn.shadowRoot?.querySelector('#icon');"
                       "  if (!iconEl) return '';"
                       "  if (iconEl.tagName === 'ICON-FROM-TABLE') {"
                       "    return iconEl.iconInfo_?.urlOrName || '';"
                       "  }"
                       "  return iconEl.getAttribute('icon') || '';"
                       "})()")
                .ExtractString();
          },
      },
      GetButton());
}

std::u16string AvatarToolbarButtonTestAccessor::GetRenderedTooltipText(
    const gfx::Point& p) {
  return std::visit(
      absl::Overload{
          [p](AvatarToolbarButton* button) {
            return button ? button->GetRenderedTooltipText(p)
                          : std::u16string();
          },
          [this](WebUIAvatarToolbarButton* button) {
            if (!button) {
              return std::u16string();
            }
            if (ShouldUseCppFallback(button)) {
              StateProvider* active_provider =
                  button->state_manager_->GetActiveStateProvider();
              return active_provider->GetAvatarTooltipText();
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return std::u16string();
            }
            return base::UTF8ToUTF16(
                content::EvalJs(
                    contents,
                    "(async () => {"
                    "  const app = document.querySelector('toolbar-app');"
                    "  if (!app) return '';"
                    "  await app.updateComplete;"
                    "  const btn = "
                    "app.shadowRoot?.querySelector('avatar-button');"
                    "  if (!btn) return '';"
                    "  await btn.updateComplete;"
                    "  const chip = "
                    "btn.shadowRoot?.querySelector('#button');"
                    "  if (!chip) return '';"
                    "  await chip.updateComplete;"
                    "  const innerButton = "
                    "chip.shadowRoot?.querySelector('#button');"
                    "  return innerButton?.title || btn.state?.tooltip || '';"
                    "})()")
                    .ExtractString());
          },
      },
      GetButton());
}

std::u16string AvatarToolbarButtonTestAccessor::GetAccessibilityLabel() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> std::u16string {
            return button ? button->GetViewAccessibility().GetCachedName()
                          : std::u16string();
          },
          [this](WebUIAvatarToolbarButton* button) -> std::u16string {
            if (!button) {
              return std::u16string();
            }
            if (ShouldUseCppFallback(button)) {
              StateProvider* active_provider =
                  button->state_manager_->GetActiveStateProvider();
              auto labels = button->state_manager_->GetAccessibilityLabels(
                  active_provider->GetText());
              return labels.first;
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return std::u16string();
            }
            return base::UTF8ToUTF16(
                content::EvalJs(
                    contents,
                    "(async () => {"
                    "  const app = document.querySelector('toolbar-app');"
                    "  if (!app) return '';"
                    "  await app.updateComplete;"
                    "  const btn = "
                    "app.shadowRoot?.querySelector('avatar-button');"
                    "  if (!btn) return '';"
                    "  await btn.updateComplete;"
                    "  const chip = "
                    "btn.shadowRoot?.querySelector('#button');"
                    "  if (!chip) return '';"
                    "  await chip.updateComplete;"
                    "  const innerButton = "
                    "chip.shadowRoot?.querySelector('#button');"
                    "  return innerButton?.getAttribute('aria-label') || '';"
                    "})()")
                    .ExtractString());
          },
      },
      GetButton());
}

std::u16string AvatarToolbarButtonTestAccessor::GetAccessibilityDescription() {
  return std::visit(
      absl::Overload{
          [](AvatarToolbarButton* button) -> std::u16string {
            return button
                       ? button->GetViewAccessibility().GetCachedDescription()
                       : std::u16string();
          },
          [this](WebUIAvatarToolbarButton* button) -> std::u16string {
            if (!button) {
              return std::u16string();
            }
            if (ShouldUseCppFallback(button)) {
              StateProvider* active_provider =
                  button->state_manager_->GetActiveStateProvider();
              auto labels = button->state_manager_->GetAccessibilityLabels(
                  active_provider->GetText());
              return labels.second;
            }
            content::WebContents* contents = GetWebContents();
            if (!contents) {
              return std::u16string();
            }
            return base::UTF8ToUTF16(
                content::EvalJs(
                    contents,
                    "(async () => {"
                    "  const app = document.querySelector('toolbar-app');"
                    "  if (!app) return '';"
                    "  await app.updateComplete;"
                    "  const btn = "
                    "app.shadowRoot?.querySelector('avatar-button');"
                    "  if (!btn) return '';"
                    "  await btn.updateComplete;"
                    "  const chip = "
                    "btn.shadowRoot?.querySelector('#button');"
                    "  if (!chip) return '';"
                    "  await chip.updateComplete;"
                    "  const innerButton = "
                    "chip.shadowRoot?.querySelector('#button');"
                    "  return innerButton?.getAttribute('aria-description') || "
                    "'';"
                    "})()")
                    .ExtractString());
          },
      },
      GetButton());
}

void LeftClickExtensionButton(content::WebContents* web_contents,
                              const std::string& id) {
  EXPECT_TRUE(content::ExecJs(
      web_contents, base::StringPrintf(kClickExtensionButtonScript, id.c_str(),
                                       "btn.click();")));
}

void RightClickExtensionButton(content::WebContents* web_contents,
                               const std::string& id) {
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      base::StringPrintf(kClickExtensionButtonScript, id.c_str(), R"(
        btn.dispatchEvent(new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          view: window,
          button: 2
        }));
      )")));
}

const char kGetCoordinatesJS[] =
    "const rect = target.getBoundingClientRect(); "
    "const x = rect.left + rect.width / 2; "
    "const y = rect.top + rect.height / 2; ";

std::string GetButtonAppJS(const std::string& selector) {
  return base::StringPrintf(
      "document.querySelector('toolbar-app')?.shadowRoot?.querySelector('%s')",
      selector.c_str());
}

bool IsButtonVisible(content::WebContents* web_contents,
                     const std::string& selector) {
  static constexpr char kScript[] = R"(
    (() => {
      const btn = %s;
      return !!btn && btn.checkVisibility();
    })();
  )";

  return content::EvalJs(
             web_contents,
             base::StringPrintf(kScript, GetButtonAppJS(selector).c_str()))
      .ExtractBool();
}

bool WaitForButtonVisible(content::WebContents* web_contents,
                          const std::string& selector) {
  return base::test::RunUntil(
      [&]() { return IsButtonVisible(web_contents, selector); });
}

bool WaitForButtonHidden(content::WebContents* web_contents,
                         const std::string& selector) {
  return base::test::RunUntil(
      [&]() { return !IsButtonVisible(web_contents, selector); });
}

void PinButton(BrowserWindowInterface* browser,
               views::WebView* web_view,
               const char* pref) {
  browser->GetProfile()->GetPrefs()->SetBoolean(pref, true);
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
}

WebUIToolbarWebView* SetUpAndPinHomeButton(BrowserWindowInterface* browser) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser);
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser, web_view, prefs::kShowHomeButton);
  EXPECT_TRUE(WaitForButtonVisible(web_view->GetWebContents(), "#home"));
  return webui_toolbar_view;
}

std::string GetButtonIconJS(const std::string& selector) {
  return base::StrCat(
      {GetButtonAppJS(selector),
       "?.shadowRoot?.querySelector('cr-icon-button, toolbar-chip-button')"});
}

std::string AddMockPointerCaptureFunctions(const char* target) {
  return base::StringPrintf(
      R"({
        var element = %s;
        var elements = [element, element?.parentElement].filter(Boolean);
        var hasCapture = null;
        for (var el of elements) {
          el.setPointerCapture = (id) => { hasCapture = id; };
          el.hasPointerCapture = (id) => { return id == hasCapture; };
          el.releasePointerCapture = (id) => {
            if (id == hasCapture || id == '*') {
              hasCapture = null;
            }
          };
        }
      })",
      target);
}

std::string DispatchEventScript(const std::string& selector,
                                const std::string& event_class,
                                const std::string& type,
                                const std::string& options) {
  return base::StringPrintf(
      "(() => { const target = %s; "
      "if (target) { "
      "  %s"
      "  %s"
      "  target.dispatchEvent(new %s('%s', "
      "  {bubbles: true, cancelable: true, view: window, clientX: x, clientY: "
      "y, "
      "  %s}));"
      "} })();",
      GetButtonIconJS(selector).c_str(), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str(), event_class.c_str(),
      type.c_str(), options.c_str());
}
