// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"

#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kChromeExtension,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kFile};

constexpr UrlIdentity::FormatOptions options = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

std::u16string GetAllowAlwaysTextInternal(
    size_t num_requests,
    permissions::PermissionRequest* first_request) {
  CHECK_GT(num_requests, 0u);
  CHECK(first_request);

  if (num_requests == 1 && first_request->GetAllowAlwaysText().has_value()) {
    // A prompt for a single request can use an "allow always" text that is
    // customized for it.
    return first_request->GetAllowAlwaysText().value();
  }

  // Use the generic text.
  return l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_WHILE_VISITING);
}

std::u16string GetBlockTextInternal(
    size_t num_requests,
    permissions::PermissionRequest* first_request) {
  CHECK_GT(num_requests, 0u);
  if (auto text = first_request->GetBlockText();
      num_requests == 1u && text.has_value()) {
    // A prompt for a single request can use a "block" text that is customized
    // for it.
    return text.value();
  }

  // Use the generic text.
  return l10n_util::GetStringUTF16(IDS_PERMISSION_NEVER_ALLOW);
}

}  // namespace

PermissionPromptBaseView::PermissionPromptBaseView(
    content::WebContents* web_contents,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate)
    : BubbleDialogDelegateView(views::BubbleAnchor(),
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               /*autosize=*/true),
      WebContentsObserver(web_contents),
      url_identity_(GetUrlIdentity(web_contents, *delegate)) {
  auto* host_widget =
      views::Widget::GetWidgetForNativeWindow(GetNativeWindow());
  record_host_always_active_value_ =
      host_widget && host_widget->ShouldPaintAsActive();

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/40084558, permission prompts should not be
  // accepted as the default action.
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kNone));

  // The host widget can be null in tests
  if (host_widget) {
    host_paint_as_active_subscription_ =
        host_widget->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &PermissionPromptBaseView::HostPaintAsActiveChanged,
            base::Unretained(this)));
  }
  request_type_ =
      permissions::PermissionUtil::GetUmaValueForRequests(delegate->Requests());
}

PermissionPromptBaseView::~PermissionPromptBaseView() {
  // `request_type_` can be unknown in tests
  if (request_type_ != permissions::RequestTypeForUma::UNKNOWN) {
    permissions::PermissionUmaUtil::RecordBrowserAlwaysActiveWhilePrompting(
        request_type_, /*embedded_permission_element_initiated*/ false,
        record_host_always_active_value_);
  }
}

void PermissionPromptBaseView::AddedToWidget() {
  if (url_identity_.type == UrlIdentity::Type::kDefault) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateTitleOriginLabel(GetWindowTitle(), GetTitleBoldedRanges()));
  }

  permissions::PermissionUmaUtil::RecordPromptShownInActiveBrowser(
      request_type_, /*embedded_permission_element_initiated*/ false,
      record_host_always_active_value_);
  StartTrackingPictureInPictureOcclusion();
}

void PermissionPromptBaseView::AnchorToPageInfoOrChip() {
  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
          web_contents());
  SetAnchor(configuration.anchor);
  // In fullscreen, `anchor` may be nullptr because the toolbar is hidden,
  // therefore anchor to the browser window instead.
  if (View* view = configuration.anchor.GetIfView()) {
    set_parent_window(view->GetWidget()->GetNativeView());
  } else if (ui::TrackedElement* element =
                 configuration.anchor.GetIfElement()) {
    set_parent_window(element->GetNativeView());
  } else if (GetNativeWindow()) {
    set_parent_window(platform_util::GetViewForWindow(GetNativeWindow()));
  }
  if (configuration.highlighted_element) {
    SetHighlightedElement(*configuration.highlighted_element);
  }
  if (configuration.anchor.IsNull()) {
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(GetBrowser()));
  }
  SetArrow(configuration.bubble_arrow);
}

bool PermissionPromptBaseView::ShouldIgnoreButtonPressedEventHandling(
    View* button,
    const ui::Event& event) const {
  // Ignore button pressed events whenever we're occluded by a
  // picture-in-picture window.
  return occluded_by_picture_in_picture_;
}

void PermissionPromptBaseView::OnOcclusionStateChanged(bool occluded) {
  // Protect from immediate input if the dialog has just become unoccluded.
  if (occluded_by_picture_in_picture_ && !occluded) {
    TriggerInputProtection();
  }
  occluded_by_picture_in_picture_ = occluded;
}

void PermissionPromptBaseView::FilterUnintenedEventsAndRunCallbacks(
    int button_id,
    const ui::Event& event) {
  if (GetDialogClientView()->IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false)) {
    return;
  }

  View* button = AsDialogDelegate()->GetExtraView()->GetViewByID(button_id);

  if (ShouldIgnoreButtonPressedEventHandling(button, event)) {
    return;
  }

  RunButtonCallback(button_id);
}

// static
UrlIdentity PermissionPromptBaseView::GetUrlIdentity(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate& delegate) {
  DCHECK(!delegate.Requests().empty());

  GURL origin_url = delegate.GetRequestingOrigin();
  // Use the full URL for Isolated Web Apps to match it with the app scope.
  GURL url = origin_url.SchemeIs(webapps::kIsolatedAppScheme) &&
                     delegate.GetAssociatedWebContents()
                 ? delegate.GetAssociatedWebContents()->GetLastCommittedURL()
                 : origin_url;

  UrlIdentity url_identity = UrlIdentity::CreateFromUrl(
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr,
      url, allowed_types, options);

  if (url_identity.type == UrlIdentity::Type::kFile) {
    // File URLs will show the same constant.
    url_identity.name =
        l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT_THIS_FILE);
  }

  return url_identity;
}
std::u16string PermissionPromptBaseView::GetAllowAlwaysText(
    const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
        visible_requests) {
  CHECK_GT(visible_requests.size(), 0u);
  return GetAllowAlwaysTextInternal(visible_requests.size(),
                                    visible_requests[0].get());
}

std::u16string PermissionPromptBaseView::GetAllowAlwaysText(
    const std::vector<base::SafeRef<permissions::PermissionRequest>>&
        visible_requests) {
  CHECK_GT(visible_requests.size(), 0u);
  return GetAllowAlwaysTextInternal(visible_requests.size(),
                                    &*visible_requests[0]);
}

std::u16string PermissionPromptBaseView::GetBlockText(
    const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
        visible_requests) {
  CHECK_GT(visible_requests.size(), 0u);
  return GetBlockTextInternal(visible_requests.size(),
                              visible_requests[0].get());
}

std::u16string PermissionPromptBaseView::GetBlockText(
    const std::vector<base::SafeRef<permissions::PermissionRequest>>&
        visible_requests) {
  CHECK_GT(visible_requests.size(), 0u);
  return GetBlockTextInternal(visible_requests.size(), &*visible_requests[0]);
}

void PermissionPromptBaseView::StartTrackingPictureInPictureOcclusion() {
  // If we're for a picture-in-picture window, then we are in an always-on-top
  // widget that should be tracked by the PictureInPictureOcclusionTracker.
  if (IsForPictureInPictureWindow()) {
    PictureInPictureOcclusionTracker* tracker =
        PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
    if (tracker) {
      tracker->OnPictureInPictureWidgetOpened(GetWidget());
    }
  }

  // Either way, we want to know if we're ever occluded by an always-on-top
  // window.
  occlusion_observation_.Observe(GetWidget());
}

const BrowserWindowInterface* PermissionPromptBaseView::GetBrowser() const {
  if (!web_contents()) {
    return nullptr;
  }
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  return tab ? tab->GetBrowserWindowInterface()
             : webui::GetBrowserWindowInterface(web_contents());
}

BrowserWindowInterface* PermissionPromptBaseView::GetBrowser() {
  return const_cast<BrowserWindowInterface*>(std::as_const(*this).GetBrowser());
}

gfx::NativeWindow PermissionPromptBaseView::GetNativeWindow() {
  BrowserWindowInterface* browser = GetBrowser();
  if (browser && browser->GetWindow()) {
    return browser->GetWindow()->GetNativeWindow();
  }
  if (web_contents()) {
    return web_contents()->GetTopLevelNativeWindow();
  }
  return gfx::NativeWindow();
}

std::vector<std::pair<size_t, size_t>>
PermissionPromptBaseView::GetTitleBoldedRanges() {
  return title_bolded_ranges_;
}
void PermissionPromptBaseView::SetTitleBoldedRanges(
    std::vector<std::pair<size_t, size_t>> bolded_ranges) {
  title_bolded_ranges_ = bolded_ranges;
}

void PermissionPromptBaseView::HostPaintAsActiveChanged() {
  auto* host_widget =
      views::Widget::GetWidgetForNativeWindow(GetNativeWindow());
  if (!host_widget || !host_widget->ShouldPaintAsActive()) {
    record_host_always_active_value_ = false;
  }
}

bool PermissionPromptBaseView::IsForPictureInPictureWindow() const {
  if (const BrowserWindowInterface* browser = GetBrowser()) {
    return browser->GetType() ==
           BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE;
  }
  return !!DocumentPipHost::FromChildWebContents(web_contents());
}

BEGIN_METADATA(PermissionPromptBaseView)
END_METADATA
