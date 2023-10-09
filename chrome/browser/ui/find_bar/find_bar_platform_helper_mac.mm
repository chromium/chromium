// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#import "chrome/browser/ui/find_bar/find_bar_platform_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/find_pasteboard.h"

namespace {

class FindBarPlatformHelperMac : public FindBarPlatformHelper {
 public:
  FindBarPlatformHelperMac(FindBarController* find_bar_controller)
      : FindBarPlatformHelper(find_bar_controller) {
    find_pasteboard_notification_observer_ = [NSNotificationCenter.defaultCenter
        addObserverForName:kFindPasteboardChangedNotification
                    object:[FindPasteboard sharedInstance]
                     queue:nil
                usingBlock:^(NSNotification*) {
                  UpdateFindBarControllerFromPasteboard();
                }];
    UpdateFindBarControllerFromPasteboard();
  }

  FindBarPlatformHelperMac(const FindBarPlatformHelperMac&) = delete;
  FindBarPlatformHelperMac& operator=(const FindBarPlatformHelperMac&) = delete;

  ~FindBarPlatformHelperMac() override {
    [NSNotificationCenter.defaultCenter
        removeObserver:find_pasteboard_notification_observer_];
  }

  void OnUserChangedFindText(std::u16string text) override {
    if (find_bar_controller_->web_contents()
            ->GetBrowserContext()
            ->IsOffTheRecord()) {
      return;
    }

    {
      base::AutoReset<bool> resetter(&sending_own_notification_, true);
      [[FindPasteboard sharedInstance]
          setFindText:base::SysUTF16ToNSString(text)];
    }
  }

 private:
  bool sending_own_notification_ = false;

  void UpdateFindBarControllerFromPasteboard() {
    content::WebContents* active_web_contents =
        find_bar_controller_->web_contents();
    Browser* browser = active_web_contents
                           ? chrome::FindBrowserWithTab(active_web_contents)
                           : nullptr;
    if (browser) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();

      for (int i = 0; i < tab_strip_model->count(); ++i) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(i);
        if (active_web_contents == web_contents)
          continue;
        find_in_page::FindTabHelper* find_tab_helper =
            find_in_page::FindTabHelper::FromWebContents(web_contents);
        find_tab_helper->StopFinding(find_in_page::SelectionAction::kClear);
      }
    }

    if (!sending_own_notification_) {
      NSString* find_text = [[FindPasteboard sharedInstance] findText];
      find_bar_controller_->SetText(base::SysNSStringToUTF16(find_text));
    }
  }

  id __strong find_pasteboard_notification_observer_;
};

}  // namespace

// static
std::unique_ptr<FindBarPlatformHelper> FindBarPlatformHelper::Create(
    FindBarController* find_bar_controller) {
  return std::make_unique<FindBarPlatformHelperMac>(find_bar_controller);
}
