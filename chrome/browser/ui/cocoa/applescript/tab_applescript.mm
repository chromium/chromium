// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/cocoa/applescript/apple_event_util.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/metrics_applescript.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/url_constants.h"
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
      chrome::mac::ValueToAppleEventDescriptor(&result_value);

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventDescriptor* reply_event =
      [manager replyAppleEventForSuspensionID:suspension_id];
  [reply_event setParamDescriptor:result_descriptor
                       forKeyword:keyDirectObject];
  [manager resumeWithSuspensionID:suspension_id];
}

}  // namespace

@interface TabAppleScript()
@property (nonatomic, copy) NSString* tempURL;
@end

@implementation TabAppleScript

@synthesize tempURL = tempURL_;

- (instancetype)init {
  if ((self = [super init])) {
    SessionID::id_type futureSessionIDOfTab = SessionID::NewUnique().id() + 1;
    // Holds the SessionID that the new tab is going to get.
    base::scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:futureSessionIDOfTab]);
    [self setUniqueID:numID];
  }
  return self;
}

- (void)dealloc {
  [tempURL_ release];
  [super dealloc];
}

- (instancetype)initWithWebContents:(content::WebContents*)webContents {
  if (!webContents) {
    [self release];
    return nil;
  }

  if ((self = [super init])) {
    // It is safe to be weak; if a tab goes away (e.g. the user closes a tab)
    // the AppleScript runtime calls tabs in AppleScriptWindow and this
    // particular tab is never returned.
    webContents_ = webContents;
    profile_ = Profile::FromBrowserContext(webContents->GetBrowserContext());
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(webContents);
    base::scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:session_tab_helper->session_id().id()]);
    [self setUniqueID:numID];
  }
  return self;
}

- (void)setWebContents:(content::WebContents*)webContents {
  DCHECK(webContents);
  // It is safe to be weak; if a tab goes away (e.g. the user closes a tab)
  // the AppleScript runtime calls tabs in AppleScriptWindow and this
  // particular tab is never returned.
  webContents_ = webContents;
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(webContents);
  profile_ = Profile::FromBrowserContext(webContents->GetBrowserContext());
  base::scoped_nsobject<NSNumber> numID(
      [[NSNumber alloc] initWithInt:session_tab_helper->session_id().id()]);
  [self setUniqueID:numID];

  if ([self tempURL])
    [self setURL:[self tempURL]];
}

- (NSString*)URL {
  if (!webContents_) {
    return nil;
  }

  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry) {
    return nil;
  }
  const GURL& url = entry->GetVirtualURL();
  return base::SysUTF8ToNSString(url.spec());
}

- (void)setURL:(NSString*)aURL {
  // If a scripter sets a URL before |webContents_| or |profile_| is set, save
  // it at a temporary location. Once they're set, -setURL: will be call again
  // with the temporary URL.
  if (!profile_ || !webContents_) {
    [self setTempURL:aURL];
    return;
  }

  GURL url(base::SysNSStringToUTF8(aURL));
  if (!chrome::mac::IsJavaScriptEnabledForProfile(profile_) &&
      url.SchemeIs(url::kJavaScriptScheme)) {
    AppleScript::SetError(AppleScript::errJavaScriptUnsupported);
    return;
  }

  // check for valid url.
  if (!url.is_empty() && !url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry)
    return;

  const GURL& previousURL = entry->GetVirtualURL();
  webContents_->OpenURL(OpenURLParams(
      url,
      content::Referrer(previousURL, network::mojom::ReferrerPolicy::kDefault),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
}

- (NSString*)title {
  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry)
    return nil;

  base::string16 title = entry ? entry->GetTitle() : base::string16();
  return base::SysUTF16ToNSString(title);
}

- (NSNumber*)loading {
  BOOL loadingValue = webContents_->IsLoading() ? YES : NO;
  return [NSNumber numberWithBool:loadingValue];
}

- (void)handlesUndoScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_UNDO);
  webContents_->Undo();
}

- (void)handlesRedoScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_REDO);
  webContents_->Redo();
}

- (void)handlesCutScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_CUT);
  webContents_->Cut();
}

- (void)handlesCopyScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_COPY);
  webContents_->Copy();
}

- (void)handlesPasteScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_PASTE);
  webContents_->Paste();
}

- (void)handlesSelectAllScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(
      AppleScript::AppleScriptCommand::TAB_SELECT_ALL);
  webContents_->SelectAll();
}

- (void)handlesGoBackScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_GO_BACK);
  NavigationController& navigationController = webContents_->GetController();
  if (navigationController.CanGoBack())
    navigationController.GoBack();
}

- (void)handlesGoForwardScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(
      AppleScript::AppleScriptCommand::TAB_GO_FORWARD);
  NavigationController& navigationController = webContents_->GetController();
  if (navigationController.CanGoForward())
    navigationController.GoForward();
}

- (void)handlesReloadScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_RELOAD);
  NavigationController& navigationController = webContents_->GetController();
  const bool checkForRepost = true;
  navigationController.Reload(content::ReloadType::NORMAL, checkForRepost);
}

- (void)handlesStopScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_STOP);
  webContents_->Stop();
}

- (void)handlesPrintScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_PRINT);
  bool initiated = printing::PrintViewManager::FromWebContents(webContents_)
                       ->PrintNow(webContents_->GetMainFrame());
  if (!initiated) {
    AppleScript::SetError(AppleScript::errInitiatePrinting);
  }
}

- (void)handlesSaveScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_SAVE);
  NSDictionary* dictionary = [command evaluatedArguments];

  NSURL* fileURL = [dictionary objectForKey:@"File"];
  // Scripter has not specifed the location at which to save, so we prompt for
  // it.
  if (!fileURL) {
    webContents_->OnSavePage();
    return;
  }

  base::FilePath mainFile(base::SysNSStringToUTF8([fileURL path]));
  // We create a directory path at the folder within which the file exists.
  // Eg.    if main_file = '/Users/Foo/Documents/Google.html'
  // then directory_path = '/Users/Foo/Documents/Google_files/'.
  base::FilePath directoryPath = mainFile.RemoveExtension();
  directoryPath = directoryPath.InsertBeforeExtension(std::string("_files/"));

  NSString* saveType = [dictionary objectForKey:@"FileType"];

  content::SavePageType savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
  if (saveType) {
    if ([saveType isEqualToString:@"only html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_ONLY_HTML;
    } else if ([saveType isEqualToString:@"complete html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
    } else {
      AppleScript::SetError(AppleScript::errInvalidSaveType);
      return;
    }
  }

  webContents_->SavePage(mainFile, directoryPath, savePageType);
}

- (void)handlesCloseScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::TAB_CLOSE);
  webContents_->GetDelegate()->CloseContents(webContents_);
}

- (void)handlesViewSourceScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(
      AppleScript::AppleScriptCommand::TAB_VIEW_SOURCE);
  NavigationEntry* entry =
      webContents_->GetController().GetLastCommittedEntry();
  if (entry) {
    webContents_->OpenURL(
        OpenURLParams(GURL(content::kViewSourceScheme + std::string(":") +
                           entry->GetURL().spec()),
                      Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_LINK, false));
  }
}

- (id)handlesExecuteJavascriptScriptCommand:(NSScriptCommand*)command {
  AppleScript::LogAppleScriptUMA(
      AppleScript::AppleScriptCommand::TAB_EXECUTE_JAVASCRIPT);

  if (!chrome::mac::IsJavaScriptEnabledForProfile(profile_)) {
    AppleScript::SetError(AppleScript::errJavaScriptUnsupported);
    return nil;
  }

  content::RenderFrameHost* frame = webContents_->GetMainFrame();
  if (!frame) {
    NOTREACHED();
    return nil;
  }

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventManagerSuspensionID suspensionID =
      [manager suspendCurrentAppleEvent];
  content::RenderFrameHost::JavaScriptResultCallback callback =
      base::BindOnce(&ResumeAppleEventAndSendReply, suspensionID);

  base::string16 script = base::SysNSStringToUTF16(
      [[command evaluatedArguments] objectForKey:@"javascript"]);
  frame->ExecuteJavaScriptInIsolatedWorld(script, std::move(callback),
                                          ISOLATED_WORLD_ID_APPLESCRIPT);

  return nil;
}

@end
