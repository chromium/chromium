// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"

namespace back_to_opener {

DEFINE_USER_DATA(BackToOpenerController);

OpenerWebContentsObserver::OpenerWebContentsObserver(
    content::WebContents* opener,
    base::WeakPtr<BackToOpenerController> controller)
    : WebContentsObserver(opener), controller_(controller) {}

OpenerWebContentsObserver::~OpenerWebContentsObserver() = default;

void OpenerWebContentsObserver::WebContentsDestroyed() {
  if (!controller_) {
    return;
  }

  controller_->ClearOpenerRelationship();
}

void OpenerWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!controller_ || !web_contents()) {
    return;
  }

  // Only check primary main frame navigations (not subframes, fenced frames,
  // portals, prerender non-primary main frames, or bfcache non-primary main
  // frames) to detect if the opener has navigated away.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  GURL current_url = navigation_handle->GetURL();

  // Clear relationship if the opener navigated to a different URL.
  // Note: This also clears the relationship for same-document navigations
  // (e.g., clicking anchor links that change the hash fragment like
  // foo.com/page#anchor). This is intentional for now to be strict about
  // maintaining the relationship only when the opener stays on the exact same
  // URL. If needed in the future, this can be relaxed to allow same-document
  // navigations by checking navigation_handle->IsSameDocument().
  GURL original_url = controller_->GetOpenerOriginalURL();
  if (!original_url.is_valid() || current_url != original_url) {
    controller_->ClearOpenerRelationship();
    // Note: This object (OpenerWebContentsObserver) will be deleted after
    // ClearOpenerRelationship() returns, as it's owned by the controller.
    return;
  }
}

// static
TabCloseObserver* TabCloseObserver::CreateForWebContents(
    content::WebContents* web_contents,
    base::WeakPtr<content::WebContents> opener_web_contents,
    base::TimeTicks close_start_time) {
  if (auto* existing = FromWebContents(web_contents)) {
    existing->close_start_time_ = close_start_time;
    existing->opener_web_contents_ = opener_web_contents;
    return existing;
  }

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new TabCloseObserver(
                         web_contents, opener_web_contents, close_start_time)));

  return FromWebContents(web_contents);
}

TabCloseObserver::TabCloseObserver(
    content::WebContents* web_contents,
    base::WeakPtr<content::WebContents> opener_web_contents,
    base::TimeTicks close_start_time)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabCloseObserver>(*web_contents),
      close_start_time_(close_start_time),
      opener_web_contents_(opener_web_contents) {}

TabCloseObserver::~TabCloseObserver() = default;

void TabCloseObserver::WebContentsDestroyed() {
  // Record the tab close duration.
  base::TimeDelta close_duration = base::TimeTicks::Now() - close_start_time_;
  base::UmaHistogramLongTimes(
      "Navigation.BackToOpener.DestinationTabCloseDuration", close_duration);

  // Activate the opener tab/window if it still exists.
  // Use WeakPtr to safely access the opener WebContents. If the opener was
  // closed before the destination tab finished closing (e.g., due to an
  // unload prompt), we won't activate it.
  content::WebContents* opener_web_contents = opener_web_contents_.get();
  if (!opener_web_contents) {
    return;
  }

  // Look up the opener's current location
  tabs::TabInterface* opener_tab =
      tabs::TabInterface::MaybeGetFromContents(opener_web_contents);
  if (!opener_tab) {
    return;
  }

  BrowserWindowInterface* opener_window =
      opener_tab->GetBrowserWindowInterface();
  if (!opener_window) {
    return;
  }

  TabStripModel* opener_strip = opener_window->GetTabStripModel();
  if (!opener_strip) {
    return;
  }

  // Activate opener tab/window now that the destination tab is closed.
  int opener_index = opener_strip->GetIndexOfWebContents(opener_web_contents);
  if (opener_index != TabStripModel::kNoTab) {
    // Verify the index is still valid before activating.
    if (opener_strip->ContainsIndex(opener_index)) {
      opener_strip->ActivateTabAt(opener_index);
    }
    ui::BaseWindow* window = opener_window->GetWindow();
    if (window && !window->IsActive()) {
      window->Activate();
    }
  }
}

BackToOpenerController::BackToOpenerController(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  pinned_state_changed_subscription_ = tab.RegisterPinnedStateChanged(
      base::BindRepeating(&BackToOpenerController::OnPinnedStateChanged,
                          weak_factory_.GetWeakPtr()));
}

BackToOpenerController::~BackToOpenerController() = default;

// static
const BackToOpenerController* BackToOpenerController::From(
    const tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

// static
BackToOpenerController* BackToOpenerController::From(tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

// static
bool BackToOpenerController::HasValidOpener(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  const BackToOpenerController* controller = From(tab);
  return controller != nullptr && controller->HasValidOpener();
}

// static
bool BackToOpenerController::CanGoBackToOpener(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  const BackToOpenerController* controller = From(tab);
  return controller != nullptr && controller->CanGoBackToOpener();
}

// static
void BackToOpenerController::GoBackToOpener(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  BackToOpenerController* controller = From(tab);
  if (controller && controller->CanGoBackToOpener()) {
    controller->GoBackToOpener();
  }
}

bool BackToOpenerController::CanGoBackToOpener() const {
  // Pinned tabs should not have back-to-opener functionality.
  // But maintain the relationship in case the tab is unpinned later.
  if (is_pinned_) {
    return false;
  }

  if (!tab().GetContents()) {
    return false;
  }

  // Rely on our cached relationship. HasLiveOriginalOpenerChain is unreliable
  // and often returns false even when a relationship exists.
  return has_valid_opener_;
}

void BackToOpenerController::GoBackToOpener() {
  if (!CanGoBackToOpener()) {
    return;
  }

  // Log that the back-to-opener feature is being used
  base::UmaHistogramBoolean("Navigation.BackToOpener.Clicked", true);

  content::WebContents* dst_contents = tab().GetContents();
  if (dst_contents && opener_web_contents_) {
    // Record the time when ClosePage() is called to measure tab close duration.
    // Create TabCloseObserver to handle duration measurement and opener
    // activation after the tab is actually destroyed (after any unload
    // prompts).
    base::TimeTicks close_start_time = base::TimeTicks::Now();
    TabCloseObserver::CreateForWebContents(dst_contents, opener_web_contents_,
                                           close_start_time);

    dst_contents->ClosePage();
  }
}

bool BackToOpenerController::HasValidOpener() const {
  return has_valid_opener_;
}

GURL BackToOpenerController::GetOpenerOriginalURL() const {
  return opener_original_url_;
}

void BackToOpenerController::OnPinnedStateChanged(tabs::TabInterface* tab,
                                                  bool new_pinned_state) {
  is_pinned_ = new_pinned_state;
  NotifyUIStateChanged();
}

void BackToOpenerController::NotifyUIStateChanged() {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }
  // Skip notifying when the tab strip selection is invalid. This controller
  // is notified on pinned state changes, which can fire during tab strip
  // reorg (e.g. unsplit or close). The selection model may be invalidated
  // (active_index() == kNoTab) at that moment.
  // TODO(crbug.com/448173940): Consider dropping the opener/destination
  // relationship when either the opener or destination tab enters split view,
  // so back-to-opener does not apply across split layout and we can avoid
  // edge cases during unsplit/close.
  BrowserWindowInterface* window = tab().GetBrowserWindowInterface();
  TabStripModel* model = window ? window->GetTabStripModel() : nullptr;
  if (!model || model->active_index() == TabStripModel::kNoTab) {
    return;
  }
  web_contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}

void BackToOpenerController::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only establish relationship on primary main frame navigations (not
  // subframes, fenced frames, portals, prerender non-primary main frames, or
  // bfcache non-primary main frames). History navigations (including bfcache
  // restorations) are excluded implicitly as they don't have initiator frames
  // from other tabs.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // A destination tab can only have one opener. If this tab already has an
  // opener relationship, don't establish a new one.
  if (has_valid_opener_) {
    return;
  }

  // Only establish relationship for user-initiated navigations (e.g., clicking
  // a link to open in a new tab). Script-initiated navigations should not
  // establish an opener relationship.
  if (!navigation_handle->HasUserGesture()) {
    return;
  }

  std::optional<blink::LocalFrameToken> initiator_token =
      navigation_handle->GetInitiatorFrameToken();
  // No initiator frame means this navigation wasn't initiated by another frame
  if (!initiator_token.has_value()) {
    return;
  }

  int initiator_process_id = navigation_handle->GetInitiatorProcessId();
  content::RenderFrameHost* initiator_frame =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(initiator_process_id,
                                              initiator_token.value()));
  if (!initiator_frame) {
    return;
  }

  // Check if the initiator frame is in a different WebContents (another tab).
  // If so, establish the opener relationship.
  content::WebContents* opener =
      content::WebContents::FromRenderFrameHost(initiator_frame);
  content::WebContents* web_contents = tab().GetContents();
  if (opener && web_contents && opener != web_contents) {
    SetOpenerWebContents(opener);
  }
}

void BackToOpenerController::SetOpenerWebContents(
    content::WebContents* opener) {
  opener_observer_ = std::make_unique<OpenerWebContentsObserver>(
      opener, weak_factory_.GetWeakPtr());
  opener_original_url_ = opener->GetLastCommittedURL();
  opener_web_contents_ = opener->GetWeakPtr();
  opener_title_ = opener->GetTitle();
  has_valid_opener_ = true;
  NotifyUIStateChanged();
}

void BackToOpenerController::ClearOpenerRelationship() {
  opener_web_contents_.reset();
  has_valid_opener_ = false;
  opener_observer_.reset();
  opener_title_.clear();

  NotifyUIStateChanged();
}

// static
std::u16string BackToOpenerController::GetFormattedOpenerTitle(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::u16string();
  }
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  const BackToOpenerController* controller = From(tab);
  if (!controller || !controller->HasValidOpener()) {
    return std::u16string();
  }

  std::u16string opener_title = controller->opener_title_;
  if (opener_title.empty()) {
    return std::u16string();
  }
  opener_title = ui::EscapeMenuLabelAmpersands(opener_title);

  // Format as "Close and go back to \"[tab title]\""
  const int kMaxBackForwardMenuWidth = 700;
  std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_HISTORY_CLOSE_AND_RETURN_TO_PREFIX, opener_title);

  // Elide the entire formatted string if needed
  formatted_text = gfx::ElideText(formatted_text, gfx::FontList(),
                                  kMaxBackForwardMenuWidth, gfx::ELIDE_TAIL);
  return formatted_text;
}

// static
ui::ImageModel BackToOpenerController::GetOpenerMenuIcon(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return ui::ImageModel();
  }
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  const BackToOpenerController* controller = From(tab);
  if (!controller || !controller->HasValidOpener()) {
    return ui::ImageModel();
  }

  content::WebContents* opener = controller->opener_web_contents_.get();
  if (!opener) {
    return ui::ImageModel();
  }

  // TODO(crbug.com/448173940): Simplify this section - it's redundant with
  // BackForwardMenuModel::GetIconAt.
  // Get the favicon from the opener's navigation entry, similar to how
  // BackForwardMenuModel does it for history entries
  content::NavigationEntry* opener_entry =
      opener->GetController().GetLastCommittedEntry();
  if (!opener_entry) {
    return ui::ImageModel();
  }

  content::FaviconStatus fav_icon = opener_entry->GetFavicon();
  if (!fav_icon.valid) {
    return ui::ImageModel();
  }

  // Only apply theming to certain chrome:// favicons, similar to history
  // entries
  if (favicon::ShouldThemifyFaviconForEntry(opener_entry)) {
    const ui::ColorProvider* const cp = &web_contents->GetColorProvider();
    gfx::ImageSkia themed_favicon = favicon::ThemeFavicon(
        fav_icon.image.AsImageSkia(), cp->GetColor(ui::kColorMenuIcon),
        cp->GetColor(ui::kColorMenuItemBackgroundHighlighted),
        cp->GetColor(ui::kColorMenuBackground));
    return ui::ImageModel::FromImageSkia(themed_favicon);
  }

  return ui::ImageModel::FromImage(fav_icon.image);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCloseObserver);

}  // namespace back_to_opener
