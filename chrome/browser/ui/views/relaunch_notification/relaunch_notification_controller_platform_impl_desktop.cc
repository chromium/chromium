// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_desktop.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the last active tabbed browser.
Browser* FindLastActiveTabbedBrowser() {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->is_type_normal()) {
      return browser;
    }
  }
  return nullptr;
}

}  // namespace

RelaunchNotificationControllerPlatformImpl::
    RelaunchNotificationControllerPlatformImpl() = default;

RelaunchNotificationControllerPlatformImpl::
    ~RelaunchNotificationControllerPlatformImpl() {
  DCHECK(!widget_);
  if (on_visible_)
    BrowserList::RemoveObserver(this);
  CHECK(!WidgetObserver::IsInObserverList());
  CHECK(!BrowserListObserver::IsInObserverList());
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRecommended(
    base::Time detection_time,
    bool /*past_deadline*/) {
  // Nothing to do if the bubble is visible.
  if (widget_)
    return;

  // Show the bubble in the most recently active browser.
  Browser* browser = FindLastActiveTabbedBrowser();
  if (!browser)
    return;

  widget_ = RelaunchRecommendedBubbleView::ShowBubble(
      browser, detection_time, base::BindRepeating(&chrome::AttemptRelaunch));

  // Monitor the widget so that |widget_| can be cleared on close.
  widget_->AddObserver(this);
}

void RelaunchNotificationControllerPlatformImpl::NotifyRelaunchRequired(
    base::Time deadline,
    base::OnceCallback<base::Time()> on_visible) {
  // Nothing to do if the dialog is visible.
  if (widget_)
    return;

  // Show the dialog in the active tabbed browser window.
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser && browser->is_type_normal()) {
    DCHECK(!on_visible_);
    ShowRequiredNotification(browser, deadline);
    return;
  }

  // If the instance is not already waiting for one to become active from a
  // previous call, start observing now.
  if (!on_visible_)
    BrowserList::AddObserver(this);

  // Hold on to the callback until an active tabbed browser is found.
  on_visible_ = std::move(on_visible);

  last_relaunch_deadline_ = deadline;
}

void RelaunchNotificationControllerPlatformImpl::CloseRelaunchNotification() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->Close();
    widget_ = nullptr;
  }
  if (on_visible_) {
    BrowserList::RemoveObserver(this);
    on_visible_.Reset();
    last_relaunch_deadline_ = base::Time();
  }
  has_shown_ = false;
}

void RelaunchNotificationControllerPlatformImpl::SetDeadline(
    base::Time deadline) {
  // Nothing to do if the dialog hasn't been shown yet (because no tabbed
  // browser has become active) or if the user has seen and dismissed the
  // dialog.
  if (widget_) {
    // The widget_ should always have a view; see https://crbug.com/324564051.
    CHECK_DEREF(RelaunchRequiredDialogView::FromWidget(widget_))
        .SetDeadline(deadline);
  }

  // Hold on to the new deadline if the instance is waiting for a Browser to
  // become active.
  if (on_visible_)
    last_relaunch_deadline_ = deadline;
}

bool RelaunchNotificationControllerPlatformImpl::IsRequiredNotificationShown()
    const {
  return widget_ != nullptr;
}

void RelaunchNotificationControllerPlatformImpl::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  widget->RemoveObserver(this);
  widget_ = nullptr;
}

void RelaunchNotificationControllerPlatformImpl::OnBrowserSetLastActive(
    Browser* browser) {
  // Ignore non-tabbed browsers.
  if (!browser->is_type_normal())
    return;

  BrowserList::RemoveObserver(this);

  base::Time new_deadline =
      has_shown_ ? last_relaunch_deadline_ : std::move(on_visible_).Run();
  DCHECK(!new_deadline.is_null());

  on_visible_.Reset();
  last_relaunch_deadline_ = base::Time();

  ShowRequiredNotification(browser, new_deadline);
}

void RelaunchNotificationControllerPlatformImpl::ShowRequiredNotification(
    Browser* browser,
    base::Time deadline) {
  widget_ = RelaunchRequiredDialogView::Show(
      browser, deadline, base::BindRepeating(&chrome::AttemptRelaunch));
  has_shown_ = true;

  // Monitor the widget so that |widget_| can be cleared on close/destruction.
  widget_->AddObserver(this);
}
