// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_mac.h"

#include "base/memory/raw_ptr.h"

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/escape.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/buildflags.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
#include "base/cxx17_backports.h"
#include "base/mac/authorization_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/browser_updater_helper_client_mac.h"
#include "chrome/updater/update_service.h"  // nogncheck
#include "chrome/updater/updater_scope.h"   // nogncheck
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)

// KeystoneObserver is a simple notification observer for Keystone status
// updates. It will be created and managed by VersionUpdaterMac.
@interface KeystoneObserver : NSObject {
 @private
  raw_ptr<VersionUpdaterMac> _versionUpdater;  // Weak.
}

// Initialize an observer with an updater. The updater owns this object.
- (instancetype)initWithUpdater:(VersionUpdaterMac*)updater;

// Notification callback, called with the status of keystone operations.
- (void)handleStatusNotification:(NSNotification*)notification;

@end  // @interface KeystoneObserver

@implementation KeystoneObserver

- (instancetype)initWithUpdater:(VersionUpdaterMac*)updater {
  if ((self = [super init])) {
    _versionUpdater = updater;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(handleStatusNotification:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)handleStatusNotification:(NSNotification*)notification {
  _versionUpdater->UpdateStatus([notification userInfo]);
}

@end  // @implementation KeystoneObserver

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
namespace {

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes < 0 || total_bytes <= 0)
    return -1;
  return 100 * base::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)

VersionUpdater* VersionUpdater::Create(
    content::WebContents* /* web_contents */) {
  return new VersionUpdaterMac;
}

VersionUpdaterMac::VersionUpdaterMac()
    : show_promote_button_(false),
      keystone_observer_([[KeystoneObserver alloc] initWithUpdater:this]) {}

VersionUpdaterMac::~VersionUpdaterMac() {}

void VersionUpdaterMac::CheckForUpdate(StatusCallback status_callback,
                                       PromoteCallback promote_callback) {
#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetUpdaterScope),
      base::BindOnce(
          [](base::RepeatingCallback<void(
                 updater::UpdaterScope,
                 const updater::UpdateService::UpdateState&)> status_callback,
             updater::UpdaterScope scope) {
            BrowserUpdaterClient::Create(scope)->CheckForUpdate(
                base::BindRepeating(status_callback, scope));
          },
          base::BindRepeating(
              &VersionUpdaterMac::UpdateStatusFromChromiumUpdater,
              weak_factory_.GetWeakPtr(), status_callback, promote_callback)));
#else
  status_callback_ = std::move(status_callback);
  promote_callback_ = std::move(promote_callback);

  KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
  if (keystone_glue && ![keystone_glue isOnReadOnlyFilesystem]) {
    AutoupdateStatus recent_status = [keystone_glue recentStatus];
    if ([keystone_glue asyncOperationPending] ||
        recent_status == kAutoupdateRegisterFailed ||
        recent_status == kAutoupdateNeedsPromotion) {
      // If an asynchronous update operation is currently pending, such as a
      // check for updates or an update installation attempt, set the status
      // up correspondingly without launching a new update check.
      //
      // If registration failed, no other operations make sense, so just go
      // straight to the error.
      UpdateStatus([[keystone_glue recentNotification] userInfo]);
    } else {
      // Launch a new update check, even if one was already completed, because
      // a new update may be available or a new update may have been installed
      // in the background since the last time the Help page was displayed.
      [keystone_glue checkForUpdate];

      // Immediately, kAutoupdateStatusNotification will be posted, with status
      // kAutoupdateChecking.
      //
      // Upon completion, kAutoupdateStatusNotification will be posted with a
      // status indicating the result of the check.
    }

    UpdateShowPromoteButton();
  } else {
    // There is no glue, or the application is on a read-only filesystem.
    // Updates and promotions are impossible.
    status_callback_.Run(DISABLED, 0, false, false, std::string(), 0,
                         std::u16string());
  }
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
}

void VersionUpdaterMac::PromoteUpdater() {
#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_PROMOTE_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  base::mac::ScopedAuthorizationRef authorization(
      base::mac::AuthorizationCreateToRunAsRoot(base::mac::NSToCFCast(prompt)));
  if (!authorization.get()) {
    VLOG(0) << "Could not get authorization to run as root.";
    return;
  }

  base::ScopedCFTypeRef<CFErrorRef> error;
  Boolean result = SMJobBless(kSMDomainSystemLaunchd,
                              base::SysUTF8ToCFStringRef(kPrivilegedHelperName),
                              authorization, error.InitializeInto());
  if (!result) {
    base::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(error));
    VLOG(0) << "Could not bless the privileged helper. Resulting error: "
            << base::SysCFStringRefToUTF8(desc);
  }

  if (!update_helper_client_) {
    update_helper_client_ =
        base::MakeRefCounted<BrowserUpdaterHelperClientMac>();
  }

  update_helper_client_->SetupSystemUpdater(base::BindOnce([](int result) {
    VLOG_IF(1, result != 0) << "There was a problem with performing the system "
                               "updater tasks. Result: "
                            << result;
  }));
#else
  // Tell Keystone to make software updates available for all users.
  [[KeystoneGlue defaultKeystoneGlue] promoteTicket];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // UpdateStatus() will be called with status kAutoupdatePromoting.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // UpdateStatus() will be called with a status indicating a result of the
  // installation attempt.
  //
  // If the promotion was successful, KeystoneGlue will re-register the ticket
  // and UpdateStatus() will be called again indicating first that
  // registration is in progress and subsequently that it has completed.
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
}

void VersionUpdaterMac::UpdateStatus(NSDictionary* dictionary) {
  AutoupdateStatus keystone_status = static_cast<AutoupdateStatus>(
      [base::mac::ObjCCastStrict<NSNumber>(dictionary[kAutoupdateStatusStatus])
          intValue]);
  std::string error_messages =
      base::SysNSStringToUTF8(base::mac::ObjCCastStrict<NSString>(
          dictionary[kAutoupdateStatusErrorMessages]));

  bool enable_promote_button = true;
  std::u16string message;

  Status status;
  switch (keystone_status) {
    case kAutoupdateRegistering:
    case kAutoupdateChecking:
      status = CHECKING;
      enable_promote_button = false;
      break;

    case kAutoupdateRegistered:
    case kAutoupdatePromoted:
      UpdateShowPromoteButton();
      // Go straight into an update check. Return immediately, this routine
      // will be re-entered shortly with kAutoupdateChecking.
      [[KeystoneGlue defaultKeystoneGlue] checkForUpdate];
      return;

    case kAutoupdateCurrent:
      status = UPDATED;
      break;

    case kAutoupdateAvailable:
      // Install the update automatically. Return immediately, this routine
      // will be re-entered shortly with kAutoupdateInstalling.
      [[KeystoneGlue defaultKeystoneGlue] installUpdate];
      return;

    case kAutoupdateInstalling:
      status = UPDATING;
      enable_promote_button = false;
      break;

    case kAutoupdateInstalled:
      status = NEARLY_UPDATED;
      break;

    case kAutoupdatePromoting:
      // TODO(mark): KSRegistration currently handles the promotion
      // synchronously, meaning that the main thread's loop doesn't spin,
      // meaning that animations and other updates to the window won't occur
      // until KSRegistration is done with promotion. This looks laggy and bad
      // and probably qualifies as "jank." For now, there just won't be any
      // visual feedback while promotion is in progress, but it should complete
      // (or fail) very quickly.  http://b/2290009.
      return;

    case kAutoupdateRegisterFailed:
      enable_promote_button = false;
      [[fallthrough]];
    case kAutoupdateCheckFailed:
    case kAutoupdateInstallFailed:
    case kAutoupdatePromoteFailed:
      status = FAILED;
      message =
          l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, keystone_status);
      break;

    case kAutoupdateNeedsPromotion: {
      status = FAILED;
      std::u16string product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
      message =
          l10n_util::GetStringFUTF16(IDS_PROMOTE_INFOBAR_TEXT, product_name);
    } break;

    default:
      NOTREACHED();
      return;
  }

  // If there are any detailed error messages being passed along by Keystone,
  // log them. If we have an error to display, include the detail messages
  // below the error in a <pre> block. Don't bother displaying detail messages
  // on a success/in-progress/indeterminate status.
  if (!error_messages.empty()) {
    VLOG(1) << "Update error messages: " << error_messages;

    if (status == FAILED) {
      if (!message.empty()) {
        message += u"<br/><br/>";
      }

      message += l10n_util::GetStringUTF16(IDS_UPGRADE_ERROR_DETAILS);
      message += u"<br/><pre>";
      message += base::UTF8ToUTF16(base::EscapeForHTML(error_messages));
      message += u"</pre>";
    }
  }

  if (status_callback_)
    status_callback_.Run(status, 0, false, false, std::string(), 0, message);

  PromotionState promotion_state;
  if (promote_callback_) {
    KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
    if (keystone_glue && [keystone_glue isAutoupdateEnabledForAllUsers]) {
      promotion_state = PROMOTED;
    } else {
      promotion_state = PROMOTE_HIDDEN;

      if (show_promote_button_) {
        promotion_state =
            enable_promote_button ? PROMOTE_ENABLED : PROMOTE_DISABLED;
      }
    }

    promote_callback_.Run(promotion_state);
  }
}

void VersionUpdaterMac::UpdateShowPromoteButton() {
  if (ObsoleteSystem::IsObsoleteNowOrSoon()) {
    // Promotion is moot upon reaching the end of the line.
    show_promote_button_ = false;
    return;
  }

  KeystoneGlue* keystone_glue = [KeystoneGlue defaultKeystoneGlue];
  AutoupdateStatus recent_status = [keystone_glue recentStatus];
  if (recent_status == kAutoupdateRegistering ||
      recent_status == kAutoupdateRegisterFailed ||
      recent_status == kAutoupdatePromoted) {
    // Promotion isn't possible at this point.
    show_promote_button_ = false;
  } else if (recent_status == kAutoupdatePromoting ||
             recent_status == kAutoupdatePromoteFailed) {
    // Show promotion UI because the user either just clicked that button or
    // because the user should be able to click it again.
    show_promote_button_ = true;
  } else {
    // Show the promote button if promotion is a possibility.
    show_promote_button_ = [keystone_glue wantsPromotion];
  }
}

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
void VersionUpdaterMac::UpdateStatusFromChromiumUpdater(
    VersionUpdater::StatusCallback status_callback,
    VersionUpdater::PromoteCallback promote_callback,
    updater::UpdaterScope scope,
    const updater::UpdateService::UpdateState& update_state) {
  VersionUpdater::Status status = VersionUpdater::Status::CHECKING;
  int progress = 0;
  std::string version;
  std::string err_message;
  bool enable_promote_button = true;

  switch (update_state.state) {
    case updater::UpdateService::UpdateState::State::kCheckingForUpdates:
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kUpdateAvailable:
      status = VersionUpdater::Status::CHECKING;
      enable_promote_button = false;
      break;
    case updater::UpdateService::UpdateState::State::kDownloading:
      progress = GetDownloadProgress(update_state.downloaded_bytes,
                                     update_state.total_bytes);
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kInstalling:
      status = VersionUpdater::Status::UPDATING;
      enable_promote_button = false;
      break;
    case updater::UpdateService::UpdateState::State::kUpdated:
      status = VersionUpdater::Status::NEARLY_UPDATED;
      break;
    case updater::UpdateService::UpdateState::State::kNoUpdate:
      status = VersionUpdater::Status::UPDATED;
      break;
    case updater::UpdateService::UpdateState::State::kUpdateError:
      status = VersionUpdater::Status::FAILED;
      // TODO(https://crbug.com/1146201): Localize error string.
      err_message = base::StringPrintf(
          "An error occurred. (Error code: %d) (Extra code: %d)",
          update_state.error_code, update_state.extra_code1);
      break;
    case updater::UpdateService::UpdateState::State::kNotStarted:
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kUnknown:
      return;
  }

  status_callback.Run(status, progress, false, false, version, 0,
                      base::UTF8ToUTF16(err_message));

  // Updater should be promoted if it meets the following criteria:
  //    1) When browser is owned by root and updater is not yet installed.
  //    2) When effective user is root and browser is not owned by root.
  //    3) When effective user is not the owner of the browser and is an
  //    administrator.
  // To check whether the system level updater is installed or not, reset the
  // update_clent with system scope and attempt to get version. If the version
  // is empty, then the updater can be assumed to not be installed. If the
  // version returns a value, then the updater is installed.
  if (promote_callback) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, base::BindOnce(&ShouldPromoteUpdater),
        base::BindOnce(
            [](base::OnceCallback<void(const std::string&)> promotion,
               bool should_promote) {
              if (should_promote) {
                BrowserUpdaterClient::Create(updater::UpdaterScope::kSystem)
                    ->GetUpdaterVersion(std::move(promotion));
              }
            },
            base::BindOnce(
                &VersionUpdaterMac::UpdatePromotionStatusFromChromiumUpdater,
                weak_factory_.GetWeakPtr(), promote_callback, scope,
                enable_promote_button)));
  }
}

void VersionUpdaterMac::UpdatePromotionStatusFromChromiumUpdater(
    VersionUpdater::PromoteCallback promote_callback,
    updater::UpdaterScope scope,
    bool enable_promote_button,
    const std::string& version) {
  promote_callback.Run(
      !version.empty() && scope == updater::UpdaterScope::kSystem
          ? VersionUpdater::PROMOTED  // Successfully communicated with the
                                      // system updater.
          : (enable_promote_button ? VersionUpdater::PROMOTE_ENABLED
                                   : VersionUpdater::PROMOTE_DISABLED));
}

#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
