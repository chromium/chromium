// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_iph_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/dictation/dictation_iph.h"
#include "chrome/browser/ui/views/dictation/text_field_filter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/view.h"

namespace dictation {

namespace {

// How long to wait after focus leaves an editable field before hiding the IPH.
// Pages often blur a field and focus another from script within a few ms.
constexpr base::TimeDelta kHideDelay = base::Milliseconds(300);

// The IPH is closed once the user has typed this many keys while it shows.
constexpr int kKeyPressesBeforeClose = 3;

// The focused field's text input type can arrive shortly after the focus
// change. Retry the medium/large field check this often, this many times.
constexpr base::TimeDelta kFieldTypeRetryDelay = base::Milliseconds(50);
constexpr int kMaxFieldTypeRetries = 5;

bool IsUserInitiatedFocus(blink::mojom::FocusType focus_type) {
  switch (focus_type) {
    case blink::mojom::FocusType::kMouse:
    case blink::mojom::FocusType::kForward:
    case blink::mojom::FocusType::kBackward:
    case blink::mojom::FocusType::kAccessKey:
    case blink::mojom::FocusType::kSpatialNavigation:
      return true;
    case blink::mojom::FocusType::kNone:
    case blink::mojom::FocusType::kScript:
    case blink::mojom::FocusType::kPage:
      return false;
  }
  NOTREACHED();
}

bool HasMicrophone() {
  return !MediaCaptureDevicesDispatcher::GetInstance()
              ->GetAudioCaptureDevices()
              .empty();
}

}  // namespace

DictationIphController::DictationIphController(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab) {
  // Safe because `this` owns the subscription, which unregisters on
  // destruction.
  tab_will_deactivate_subscription_ = tab.RegisterWillDeactivate(
      base::BindRepeating(&DictationIphController::OnTabWillDeactivate,
                          base::Unretained(this)));
  // Start audio device enumeration early so the cached list is populated by the
  // time the user focuses a field.
  MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
}

DictationIphController::~DictationIphController() = default;

void DictationIphController::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  if (details.editable_level == content::EditableLevel::kNotEditable ||
      details.focus_type == blink::mojom::FocusType::kNone) {
    // Many pages blur a field and then focus another one from script right
    // away (e.g. a search box that swaps in a different input on click). Wait
    // briefly before hiding so the IPH isn't closed by that blur.
    if (anchor_) {
      // Safe because `hide_timer_` is owned by `this` and stops on
      // destruction.
      hide_timer_.Start(FROM_HERE, kHideDelay,
                        base::BindOnce(&DictationIphController::HideAnchor,
                                       base::Unretained(this)));
    }
    return;
  }

  // An editable field has focus again, so don't hide.
  hide_timer_.Stop();

  // Only the user focusing a field can show the IPH. Script-driven focus only
  // moves an anchor that is already visible.
  const bool is_user_focus = IsUserInitiatedFocus(details.focus_type);
  if (!is_user_focus && !anchor_) {
    return;
  }

  // The user moved to a different field while the IPH was showing. That counts
  // as leaving the field, i.e. the IPH was ignored.
  if (is_user_focus && !IsAnchoredNode(details.global_dom_node_id)) {
    IgnorePromo();
  }
  anchored_node_ = details.global_dom_node_id;
  content::RenderFrameHost* const frame =
      details.global_dom_node_id.document.AsRenderFrameHostIfValid();
  anchored_frame_id_ =
      frame ? frame->GetGlobalId() : content::GlobalRenderFrameHostId();

  if (!tab().IsActivated() || details.node_bounds_in_screen.IsEmpty()) {
    return;
  }

  views::View* const host = GetContentsWebView();
  if (!host) {
    return;
  }

  // `ViewSubregionAnchor` takes bounds local to its host view.
  gfx::Point origin = details.node_bounds_in_screen.origin();
  views::View::ConvertPointFromScreen(host, &origin);
  gfx::Rect local_bounds(origin, details.node_bounds_in_screen.size());

  // Only anchor to the part of the field that is visible in the page.
  local_bounds.Intersect(host->GetLocalBounds());
  if (local_bounds.IsEmpty()) {
    HideAnchor();
    return;
  }

  if (!anchor_) {
    anchor_ = std::make_unique<views::ViewSubregionAnchor>(
        kDictationIphAnchorElementId, *host);
  }
  // `MoveTo()` also handles the tab moving to a different `ContentsWebView`
  // (e.g. split view). If a promo is already showing it follows the new bounds.
  anchor_->MoveTo(*host, local_bounds);

  // Script-driven focus may have moved the anchor to a different field, so
  // check that field too. Only the user focusing a field starts the IPH.
  MaybeShowPromo(kMaxFieldTypeRetries, /*can_start_promo=*/is_user_focus);
}

void DictationIphController::MaybeShowPromo(int retries_left,
                                            bool can_start_promo) {
  if (!anchor_ || !anchored_node_) {
    return;
  }
  content::RenderFrameHost* const rfh =
      anchored_node_->document.AsRenderFrameHostIfValid();
  if (!rfh) {
    return;
  }

  // Only show the IPH on medium to large fields (textareas and rich editors),
  // where dictating longer text is useful.
  switch (GetFocusedElementTextFieldSize(*rfh)) {
    case TextFieldSize::kUnknown:
      // The field's text input type hasn't arrived yet, so retry. A retry
      // pending from a user focus also checks the new field, so a script focus
      // doesn't replace it.
      if (retries_left > 0 &&
          (can_start_promo || !field_type_retry_timer_.IsRunning())) {
        // Safe because `field_type_retry_timer_` is owned by `this` and stops
        // on destruction.
        field_type_retry_timer_.Start(
            FROM_HERE, kFieldTypeRetryDelay,
            base::BindOnce(&DictationIphController::MaybeShowPromo,
                           base::Unretained(this), retries_left - 1,
                           can_start_promo));
      }
      return;
    case TextFieldSize::kNotEditable:
    case TextFieldSize::kSmall:
      // Don't leave a queued or showing IPH on a small field.
      HideAnchor();
      return;
    case TextFieldSize::kMediumOrLarge:
      break;
  }

  // Users who already use dictation or have no microphone are not shown the
  // IPH.
  if (!can_start_promo || HasUsedDictation() || !HasMicrophone()) {
    return;
  }

  if (auto* const user_education = GetUserEducation()) {
    key_presses_while_showing_ = 0;
    // The body refers to the context menu item by name, which is branded.
    user_education::FeaturePromoParams params(
        feature_engagement::kIPHDictationFeature);
    params.body_params =
        l10n_util::GetStringUTF16(IDS_DICTATION_CONTEXT_MENU_STRING);
    user_education->MaybeShowFeaturePromo(std::move(params));
  }
}

void DictationIphController::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kRawKeyDown &&
      event.GetType() != blink::WebInputEvent::Type::kKeyDown) {
    return;
  }
  if (!IsPromoActive()) {
    key_presses_while_showing_ = 0;
    return;
  }
  // The user started typing instead of interacting with the IPH.
  if (++key_presses_while_showing_ >= kKeyPressesBeforeClose) {
    IgnorePromo();
  }
}

void DictationIphController::PrimaryPageChanged(content::Page& page) {
  HideAnchor();
}

void DictationIphController::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  // The frame holding the field is no longer the active document, e.g. it
  // navigated (each document gets a new RenderFrameHost) or went into the
  // back/forward cache. Unlike `PrimaryPageChanged()`, this also catches an
  // iframe navigating. The focus change from the old document can't be relied
  // on here, since `WebContents` ignores it once the frame isn't current.
  if (render_frame_host->GetGlobalId() == anchored_frame_id_ &&
      new_state != content::RenderFrameHost::LifecycleState::kActive) {
    HideAnchor();
  }
}

void DictationIphController::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // The frame holding the field was deleted, e.g. its iframe was removed.
  if (render_frame_host->GetGlobalId() == anchored_frame_id_) {
    HideAnchor();
  }
}

void DictationIphController::OnTabWillDeactivate(tabs::TabInterface* tab) {
  HideAnchor();
}

views::View* DictationIphController::GetContentsWebView() {
  // The tab has no window while it is detached, e.g. while it is dragged to
  // another window.
  BrowserWindowInterface* const browser = tab().GetBrowserWindowInterface();
  if (!browser) {
    return nullptr;
  }
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return nullptr;
  }
  for (ContentsWebView* const view :
       browser_view->GetAllVisibleContentsWebViews()) {
    if (view->web_contents() == web_contents()) {
      return view;
    }
  }
  return nullptr;
}

BrowserUserEducationInterface* DictationIphController::GetUserEducation()
    const {
  return BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
      web_contents());
}

bool DictationIphController::IsPromoActive() const {
  auto* const user_education = GetUserEducation();
  return user_education && user_education->IsFeaturePromoActive(
                               feature_engagement::kIPHDictationFeature);
}

bool DictationIphController::HasUsedDictation() const {
  const Profile* const profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(
      prefs::kPrefDictationOnboardingCompleted);
}

bool DictationIphController::IsAnchoredNode(
    const content::GlobalDOMNodeId& node) const {
  return anchored_node_ &&
         anchored_node_->target_element_dom_id == node.target_element_dom_id &&
         anchored_node_->document.AsRenderFrameHostIfValid() ==
             node.document.AsRenderFrameHostIfValid();
}

void DictationIphController::IgnorePromo() {
  key_presses_while_showing_ = 0;
  // This also cancels the IPH if it is still queued. Aborting a showing IPH
  // neither dismisses nor snoozes it. The user education framework only has
  // the last show time, so the IPH is blocked for
  // `user_education::features::GetAbortCooldown()` (4 hours) after this.
  // See `FeaturePromoLifecycle::CanShowSnoozePromo()`.
  if (auto* const user_education = GetUserEducation()) {
    user_education->AbortFeaturePromo(feature_engagement::kIPHDictationFeature);
  }
}

void DictationIphController::HideAnchor() {
  // Leaving the field, navigating, or switching tabs while the IPH is showing
  // counts as ignoring it. Close the IPH before destroying its anchor.
  IgnorePromo();
  anchored_node_.reset();
  anchored_frame_id_ = content::GlobalRenderFrameHostId();
  field_type_retry_timer_.Stop();
  hide_timer_.Stop();
  // Destroy rather than hide the anchor, so it never outlives its host
  // `ContentsWebView` (e.g. when the tab moves to another window).
  anchor_.reset();
}

}  // namespace dictation
