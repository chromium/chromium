// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/applescript/apple_event_util.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::Referrer;
using content::WebContents;

namespace {

void ResumeAppleEventAndSendReply(NSAppleEventManagerSuspensionID suspension_id,
                                  base::Value result_value) {
  NSAppleEventDescriptor* result_descriptor =
      chrome::mac::ValueToAppleEventDescriptor(result_value);

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventDescriptor* reply_event =
      [manager replyAppleEventForSuspensionID:suspension_id];
  [reply_event setParamDescriptor:result_descriptor
                       forKeyword:keyDirectObject];
  [manager resumeWithSuspensionID:suspension_id];
}

}  // namespace

@interface TabAppleScript ()

// Contains the temporary URL when a user creates a new folder/item with the URL
// specified like:
//
//   make new tab with properties {URL:"http://google.com"}
@property (nonatomic, copy) NSString* tempURL;

- (bool)isJavaScriptEnabled;

@end

@implementation TabAppleScript {
  // A note about lifetimes: It's not expected that this object will ever be
  // deleted behind the back of this class. AppleScript does not hold onto
  // objects between script runs; it will retain the object specifier, and if
  // needed again, AppleScript will re-iterate over the objects, and look for
  // the specified object. However, there's no hard guarantee that a race
  // couldn't be made to happen, and in tests things are torn down at odd times,
  // so it's best to use a real weak pointer.
  base::WeakPtr<content::WebContents> _webContents;
}

@synthesize tempURL = _tempURL;

- (bool)isJavaScriptEnabled {
  if (!_webContents) {
    return false;
  }

  return chrome::mac::IsJavaScriptEnabledForProfile(
      Profile::FromBrowserContext(_webContents->GetBrowserContext()));
}

- (instancetype)init {
  if ((self = [super init])) {
    // Holds the SessionID that the new tab is going to get.
    SessionID::id_type futureSessionIDOfTab = SessionID::NewUnique().id() + 1;
    self.uniqueID = [NSString stringWithFormat:@"%d", futureSessionIDOfTab];
  }
  return self;
}

- (instancetype)initWithWebContents:(content::WebContents*)webContents {
  if (!webContents) {
    return nil;
  }

  if ((self = [super init])) {
    _webContents = webContents->GetWeakPtr();

    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(webContents);
    self.uniqueID = [NSString
        stringWithFormat:@"%d", session_tab_helper->session_id().id()];
  }
  return self;
}

- (void)setWebContents:(content::WebContents*)webContents {
  DCHECK(webContents);
  _webContents = webContents->GetWeakPtr();

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(webContents);
  self.uniqueID =
      [NSString stringWithFormat:@"%d", session_tab_helper->session_id().id()];

  if (self.tempURL) {
    self.URL = self.tempURL;
  }
}

- (NSString*)URL {
  if (!_webContents) {
    return nil;
  }

  NavigationEntry* entry = _webContents->GetController().GetActiveEntry();
  if (!entry) {
    return nil;
  }
  const GURL& url = entry->GetVirtualURL();
  return base::SysUTF8ToNSString(url.spec());
}

- (void)setURL:(NSString*)url {
  // If a scripter sets a URL before |webContents_| or |profile_| is set, save
  // it at a temporary location. Once they're set, -setURL: will be call again
  // with the temporary URL.
  if (!_webContents) {
    self.tempURL = url;
    return;
  }

  GURL gurl(base::SysNSStringToUTF8(url));
  if (![self isJavaScriptEnabled] && gurl.SchemeIs(url::kJavaScriptScheme)) {
    AppleScript::SetError(AppleScript::Error::kJavaScriptUnsupported);
    return;
  }

  // Check if the URL is valid; if not, then attempting to open it will trip
  // a fatal check in navigation.
  if (!gurl.is_valid()) {
    AppleScript::SetError(AppleScript::Error::kInvalidURL);
    return;
  }

  NavigationEntry* entry = _webContents->GetController().GetActiveEntry();
  if (!entry) {
    return;
  }

  _webContents->OpenURL(OpenURLParams(gurl, content::Referrer(),
                                      WindowOpenDisposition::CURRENT_TAB,
                                      ui::PAGE_TRANSITION_TYPED, false),
                        /*navigation_handle_callback=*/{});
}

- (NSString*)title {
  if (!_webContents) {
    return nil;
  }

  NavigationEntry* entry = _webContents->GetController().GetActiveEntry();
  if (!entry) {
    return nil;
  }

  std::u16string title = entry ? entry->GetTitle() : std::u16string();
  return base::SysUTF16ToNSString(title);
}

- (NSNumber*)loading {
  if (!_webContents) {
    return nil;
  }

  BOOL loadingValue = _webContents->IsLoading() ? YES : NO;
  return @(loadingValue);
}

- (void)handlesUndoScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Undo();
}

- (void)handlesRedoScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Redo();
}

- (void)handlesCutScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Cut();
}

- (void)handlesCopyScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Copy();
}

- (void)handlesPasteScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Paste();
}

- (void)handlesSelectAllScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->SelectAll();
}

- (void)handlesGoBackScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  NavigationController& navigationController = _webContents->GetController();
  if (navigationController.CanGoBack()) {
    navigationController.GoBack();
  }
}

- (void)handlesGoForwardScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  NavigationController& navigationController = _webContents->GetController();
  if (navigationController.CanGoForward()) {
    navigationController.GoForward();
  }
}

- (void)handlesReloadScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  NavigationController& navigationController = _webContents->GetController();
  navigationController.Reload(content::ReloadType::NORMAL,
                              /*check_for_repost=*/true);
}

- (void)handlesStopScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->Stop();
}

- (void)handlesPrintScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  bool initiated =
      printing::PrintViewManager::FromWebContents(_webContents.get())
          ->PrintNow(_webContents->GetPrimaryMainFrame());
  if (!initiated) {
    AppleScript::SetError(AppleScript::Error::kInitiatePrinting);
  }
}

- (void)handlesSaveScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  NSDictionary* dictionary = command.evaluatedArguments;

  NSURL* fileURL = dictionary[@"File"];
  // Scripter has not specified the location at which to save, so we prompt for
  // it.
  if (!fileURL) {
    _webContents->OnSavePage();
    return;
  }

  base::FilePath mainFile = base::apple::NSURLToFilePath(fileURL);
  // We create a directory path at the folder within which the file exists.
  // Eg.    if main_file = '/Users/Foo/Documents/Google.html'
  // then directory_path = '/Users/Foo/Documents/Google_files/'.
  base::FilePath directoryPath = mainFile.RemoveExtension();
  directoryPath = directoryPath.InsertBeforeExtension(std::string("_files/"));

  NSString* saveType = dictionary[@"FileType"];

  content::SavePageType savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
  if (saveType) {
    if ([saveType isEqualToString:@"only html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_ONLY_HTML;
    } else if ([saveType isEqualToString:@"complete html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
    } else {
      AppleScript::SetError(AppleScript::Error::kInvalidSaveType);
      return;
    }
  }

  _webContents->SavePage(mainFile, directoryPath, savePageType);
}

- (void)handlesCloseScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->GetDelegate()->CloseContents(_webContents.get());
}

- (void)handlesViewSourceScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return;
  }

  _webContents->GetPrimaryMainFrame()->ViewSource();
}

- (id)handlesExecuteJavascriptScriptCommand:(NSScriptCommand*)command {
  if (!_webContents) {
    return nil;
  }

  if (![self isJavaScriptEnabled]) {
    AppleScript::SetError(AppleScript::Error::kJavaScriptUnsupported);
    return nil;
  }

  content::RenderFrameHost* frame = _webContents->GetPrimaryMainFrame();
  if (!frame) {
    return nil;
  }

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventManagerSuspensionID suspensionID =
      [manager suspendCurrentAppleEvent];
  content::RenderFrameHost::JavaScriptResultCallback callback =
      base::BindOnce(&ResumeAppleEventAndSendReply, suspensionID);

  std::u16string script =
      base::SysNSStringToUTF16(command.evaluatedArguments[@"javascript"]);
  frame->ExecuteJavaScriptInIsolatedWorld(script, std::move(callback),
                                          ISOLATED_WORLD_ID_APPLESCRIPT);

  return nil;
}

@end
