// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/ks_admin/ks_admin.h"

#import <Foundation/Foundation.h>
#import <getopt.h>

#import <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/mac/update_service_proxy.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"

@interface NSDate (GenerousFormat)
// Convert a string of the format "05/10/2009 12:46 PM" to an NSDate.
// Returns `nil` if the date can't be parsed.
+ (NSDate*)dateFromShortStyleString:(NSString*)dateString;
@end

@implementation NSDate (GenerousFormat)
+ (NSDate*)dateFromShortStyleString:(NSString*)dateString {
  NSDateFormatter* fmt = [[[NSDateFormatter alloc] init] autorelease];

  // Parse a string like "05/10/2009 12:46 PM".
  [fmt setFormatterBehavior:NSDateFormatterBehavior10_4];
  [fmt setDateStyle:NSDateFormatterShortStyle];
  [fmt setTimeStyle:NSDateFormatterShortStyle];
  // Apparently needed on 10.9 to avoid apparent new NSDateFormatter
  // strictness. Sample error:
  // "Error parsing date '03/14/2008 1:59 AM'.  Use MM/DD/YYYY HH:MI AMPM
  // format" Which appears incorrect, ('I' is deprecated), but this seems to
  // fix.
  [fmt setLenient:YES];

  [fmt setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

  NSDate* date = [fmt dateFromString:dateString];
  return date;
}
@end

namespace updater {
namespace {

class KSAdminApp : public App {
 public:
  KSAdminApp(int argc, char* argv[])
      : argc_(argc),
        argv_(argv),
        service_proxy_(
            base::MakeRefCounted<UpdateServiceProxy>(GetUpdaterScope())) {}

 private:
  ~KSAdminApp() override = default;
  void FirstTaskRun() override;

  bool ParseCommandLineOptions();
  NSString* ShortOptionsList(struct option* options_list, int count);
  void CheckForUpdates(base::OnceCallback<void(int)> callback);
  void Register(base::OnceCallback<void(int)> callback);
  void PrintRequestedInfo();
  void PrintUsage();
  void PrintUsageWithError(const std::string& error_message);
  void PrintVersion();
  void RequireBothOrNoneForPathAndKey(const std::string& path,
                                      const std::string& key,
                                      const std::string& kind);

  bool NewTicketValuesSpecified();
  void UpdateTicket();
  void PrintTickets();

  int argc_;
  char** argv_;

  std::string app_id_;
  std::string version_;
  std::string version_path_;
  std::string version_key_;
  std::string brand_path_;
  std::string brand_key_;
  std::string tag_;
  std::string tag_path_;
  std::string tag_key_;
  std::string xc_path_;
  std::string xcls_bundle_;
  std::string xc_query_;
  std::string url_;
  std::string cohort_hint_;
  base::Time creation_date_;

  bool register_ticket_ = false;
  bool delete_ticket_ = false;
  bool print_effective_policies_ = false;
  bool print_version_ = false;
  bool print_tickets_ = false;
  bool print_usage_ = false;
  bool list_updates_ = false;
  bool install_updates_ = false;
  bool print_tag_ = false;
  bool print_brand_ = false;
  bool user_initiated_ = false;

  scoped_refptr<UpdateServiceProxy> service_proxy_;
};

// Return true if any ticket parameter was specified in the app arguments
bool KSAdminApp::NewTicketValuesSpecified() {
  return !url_.empty() || !version_.empty() || !creation_date_.is_null() ||
         !tag_.empty() || !tag_path_.empty() || !tag_key_.empty() ||
         !brand_path_.empty() || !brand_key_.empty() ||
         !version_path_.empty() || !version_key_.empty() ||
         !cohort_hint_.empty();
}

void KSAdminApp::PrintRequestedInfo() {
  if (print_usage_)
    PrintUsage();
}

void KSAdminApp::PrintUsage() {
  const std::string usage_message =
      "Usage: ksadmin [options]\n"
      "\n"
      // Controls what action to take.
      "  --delete,-d         Delete a ticket as specified by option "
      "--productid. Deprecated.\n"
      "  --list,-l           List the available updates, but don't install "
      "any\n"
      "  --print-brand,-D    Get the brand code for a particular product.  "
      "Requires -P. Deprecated.\n"
      "  --print-policies,-m Print policies that will be applied. Deprecated.\n"
      "  --print-tag,-G      Get the tag for a particular product.  "
      "Requires "
      "-P. Deprecated.\n"
      "  --print-tickets,-p  Print all tickets and exit.  Can be "
      "abbreviated "
      "--print. Deprecated.\n"
      "  --print-version,-o  Evaluate the version version path, and version "
      "tag, and print the effective version.  Requires -P\n"
      "  --register,-r       Register a new ticket as specified by "
      "options.\n"
      "                      Register requires 4 ticket options: -P, -v, "
      "-u, "
      "and an --xcsomething\n"
      "  --brand-key,-b      Set the brand code path key.  Requires -P and "
      "-B\n"
      "  --brand-path,-B     Set the brand code path.  Requires -P and -b.  "
      "Specify empty string to remove\n"
      "  --creation-date,-c  Set the creation date of the ticket\n"
      "                      Format should be like '1/13/1961 1:13 PM'\n"
      "  --tag,-g TAG        Set the ticket's tag.  Specify empty string to "
      "remove existing tag\n"
      "  --tag-path,-H       Set the tag path.  Requires -P and -K.  "
      "Specify "
      "empty string to remove\n"
      "  --tag-key,-K        Set the tag path key.  Requires -P and -H\n"
      "  --version,-v VERS   You can also specify -P id and -v VERS to "
      "update "
      "an existing ticket's version\n"
      "  --version-path,-a   Set the version path.  Requires -P and -e.  "
      "Specify empty string to remove\n"
      "  --version-key,-e    Set the version path key.  Requires -P and -a\n"
      // Settings for those actions.
      "  --productid,-P id   ProductID; can be a GUID or a BundleID\n"
      "  --store,-s FILE     Use FILE instead of the default ticket store.\n"
      "                      Default means the system-wide if running as "
      "root, "
      "or\n"
      "                      a per-user one if running as non-root\n"
      "                      (either a per-user or system-wide one)\n"
      "  --system-store,-S   Use the system-wide ticket store\n"
      "  --user-store,-U     Use a per-user ticket store\n"
      "  --user-initiated,-F This operation is initiated by a user; sets "
      "the "
      "ondemand tag for update checks\n"
      "  --xcpath,-x PATH    Specify an existence checker that checks for "
      "the "
      "existence\n"
      "                      of the given path\n"
      "  --xclsbundle,-X BID Specify an existence checker that asks "
      "LaunchServices whether an\n"
      "                      application exists with the given Bundle ID\n"
      "  --xcquery, -q query Specify an existence checker that checks "
      "whether "
      "the given Spotlight\n"
      "                      query returns any results\n"
      "  --url,-u URL        You can also specify -P id and -u URL to "
      "update "
      "an existing ticket's url\n"
      "  --cohortHint, -C hint Hint to the update server to move the "
      "application to the "
      "specified cohort\n"
      "  --verbose,-V        Print activities verbosely. Deprecated.\n"
      "  --help,-h           Print this message\n";
  printf("%s\n", usage_message.c_str());
}

void KSAdminApp::PrintUsageWithError(const std::string& error_message) {
  LOG(ERROR) << error_message;
  PrintUsage();
}

void KSAdminApp::RequireBothOrNoneForPathAndKey(const std::string& path,
                                                const std::string& key,
                                                const std::string& kind) {
  // `path` and `key` must be specified as a pair.
  if (path.empty() != key.empty()) {
    PrintUsageWithError(base::StringPrintf(
        "Both %s path and key must be supplied.", kind.c_str()));
    Shutdown(1);
  }
}

void KSAdminApp::Register(base::OnceCallback<void(int)> callback) {
  RegistrationRequest registration;
  registration.app_id = app_id_;
  registration.brand_code = brand_path_;
  registration.tag = tag_;
  registration.version = base::Version(version_);
  registration.existence_checker_path = base::FilePath(xc_path_);

  service_proxy_->RegisterApp(
      registration, base::BindOnce(
                        [](base::OnceCallback<void(int)> cb,
                           const RegistrationResponse& response) {
                          if (response.status_code == kRegistrationSuccess) {
                            std::move(cb).Run(0);
                          } else {
                            LOG(ERROR) << "Updater registration error: "
                                       << response.status_code;
                            std::move(cb).Run(1);
                          }
                        },
                        std::move(callback)));
}

void KSAdminApp::CheckForUpdates(base::OnceCallback<void(int)> callback) {
  if (app_id_.empty()) {
    LOG(ERROR) << "Missing --productid.";
    std::move(callback).Run(1);
    return;
  }

  service_proxy_->Update(
      app_id_,
      user_initiated_ ? UpdateService::Priority::kForeground
                      : UpdateService::Priority::kBackground,
      base::BindRepeating([](UpdateService::UpdateState update_state) {
        if (update_state.state == UpdateService::UpdateState::State::kUpdated) {
          printf("Finished updating (errors=%d reboot=%s)\n", 0, "YES");
        }
      }),
      base::BindOnce(
          [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            if (result == UpdateService::Result::kSuccess) {
              printf("Available updates: (\n)\n");
              std::move(cb).Run(0);
            } else {
              LOG(ERROR) << "Error code: " << result;
              std::move(cb).Run(1);
            }
          },
          std::move(callback)));
}

void KSAdminApp::UpdateTicket() {
  // TODO(crbug.com/1244983): Implement?
  if (!app_id_.empty() && NewTicketValuesSpecified()) {
  }
}

NSString* KSAdminApp::ShortOptionsList(struct option* options_list, int count) {
  NSMutableString* s = [NSMutableString string];
  for (int x = 0; x < count; x++) {
    [s appendFormat:@"%c", (char)options_list[x].val];
    if (options_list[x].has_arg != no_argument)
      [s appendString:@":"];
  }
  return s;
}

bool KSAdminApp::ParseCommandLineOptions() {
  static option g_command_line_long_options[] = {
      {"version-path", required_argument, nullptr, 'a'},
      {"brand-path", required_argument, nullptr, 'B'},
      {"brand-key", required_argument, nullptr, 'b'},

      {"creation-date", required_argument, nullptr, 'c'},
      {"print-brand", no_argument, nullptr, 'D'},
      {"delete", no_argument, nullptr, 'd'},

      {"version-key", required_argument, nullptr, 'e'},
      {"user-initiated", no_argument, nullptr, 'F'},

      {"print-tag", no_argument, nullptr, 'G'},
      // Intentionally there is no 'preserve-tag', since it's always
      // preserved.  Tags could stick around forever (say for Chrome release
      // channels), but TTTs are shorter lived, and eventually removed by
      // the product.
      {"tag", required_argument, nullptr, 'g'},
      {"tag-path", required_argument, nullptr, 'H'},
      {"help", no_argument, nullptr, 'h'},

      {"install", no_argument, nullptr, 'i'},

      {"tag-key", required_argument, nullptr, 'K'},
      {"ksadmin-version", no_argument, nullptr, 'k'},
      {"list", no_argument, nullptr, 'l'},

      {"print-policies", no_argument, nullptr, 'm'},
      {"print-version", no_argument, nullptr, 'o'},
      {"productid", required_argument, nullptr, 'P'},
      {"print", no_argument, nullptr, 'p'},
      {"print-tickets", no_argument, nullptr, 'p'},

      {"xcquery", required_argument, nullptr, 'q'},
      {"register", no_argument, nullptr, 'r'},
      {"system-store", no_argument, nullptr, 'S'},
      {"store", required_argument, nullptr, 's'},
      {"tttoken", no_argument, nullptr, 'T'},           // Deprecated
      {"preserve-tttoken", no_argument, nullptr, 't'},  // Deprecated
      {"user-store", no_argument, nullptr, 'U'},
      {"url", required_argument, nullptr, 'u'},
      {"verbose", no_argument, nullptr, 'V'},
      {"version", required_argument, nullptr, 'v'},
      {"xcpath", required_argument, nullptr, 'x'},
      {"xclsbundle", required_argument, nullptr, 'X'},
      {"cohortHint", required_argument, nullptr, 'C'},

      {nullptr, 0, nullptr, 0},
  };

  const std::string short_opt_string = base::SysNSStringToUTF8(ShortOptionsList(
      g_command_line_long_options, base::size(g_command_line_long_options)));
  const char* short_opts = short_opt_string.c_str();
  NSString* store_path = nil;
  bool use_system_store = false;
  bool use_user_store = false;
  int c = 0;
  while ((c = getopt_long(argc_, argv_, short_opts, g_command_line_long_options,
                          nullptr)) != -1) {
    switch (c) {
      case 's':
        if (store_path != nil) {
          PrintUsageWithError("Cannot specify multiple ticket stores.");
          return false;
        }
        store_path = [NSString stringWithUTF8String:optarg];
        break;
      case 'S':
        use_system_store = true;
        break;
      case 'U':
        use_user_store = true;
        break;
      case 'r':
        register_ticket_ = true;
        break;
      case 'm':
        print_effective_policies_ = true;
        break;
      case 'd':
        delete_ticket_ = true;
        break;
      case 'P':
        app_id_ =
            base::SysNSStringToUTF8([[NSString stringWithUTF8String:optarg]
                stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]]);
        break;
      case 'v':
        version_ =
            base::SysNSStringToUTF8([[NSString stringWithUTF8String:optarg]
                stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]]);
        break;
      case 'a':
        version_path_ = optarg;
        break;
      case 'e':
        version_key_ = optarg;
        break;
      case 'o':
        print_version_ = true;
        break;
      case 'x':
        xc_path_ = optarg;
        break;
      case 'X':
        xcls_bundle_ = optarg;
        break;
      case 'q':
        xc_query_ = optarg;
        break;
      case 'u':
        url_ = optarg;
        break;
      case 'p':
        print_tickets_ = true;
        break;
      case 'l':
        list_updates_ = true;
        break;
      case 'i':
        install_updates_ = true;
        break;
      case 'g':
        tag_ = optarg;
        break;
      case 'G':
        print_tag_ = true;
        break;
      case 'H':
        tag_path_ = optarg;
        break;
      case 'K':
        tag_key_ = optarg;
        break;
      case 'B':
        brand_path_ = optarg;
        break;
      case 'b':
        brand_key_ = optarg;
        break;
      case 'D':
        print_brand_ = true;
        break;
      case 'c': {
        @autoreleasepool {
          NSDate* date = [NSDate
              dateFromShortStyleString:[NSString stringWithUTF8String:optarg]];
          if (date == nil) {
            return false;
          }
          creation_date_ = base::Time::FromNSDate(date);
        }
      } break;
      case 'k':
        print_version_ = true;
        break;
      case 'F':
        user_initiated_ = true;
        break;
      case 'C':
        cohort_hint_ =
            base::SysNSStringToUTF8([[NSString stringWithUTF8String:optarg]
                stringByTrimmingCharactersInSet:
                    [NSCharacterSet whitespaceAndNewlineCharacterSet]]);
        break;
      case 't':
      case 'T':
      case 'V':
        break;
      case 'h':
      case '?':
      default:
        PrintUsage();
        break;
    }
  }

  RequireBothOrNoneForPathAndKey(tag_path_, tag_key_, "tag");
  RequireBothOrNoneForPathAndKey(brand_path_, brand_key_, "brand");
  RequireBothOrNoneForPathAndKey(version_path_, version_key_, "version");

  if (!(register_ticket_ || delete_ticket_ || print_version_ ||
        print_effective_policies_ || print_tag_ || print_brand_ ||
        print_tickets_ || list_updates_ || install_updates_) &&
      !(!app_id_.empty() && NewTicketValuesSpecified())) {
    PrintUsage();
    return false;
  }

  if (register_ticket_ && delete_ticket_) {
    LOG(ERROR) << "Can't register and delete a ticket at the same time.";
    return false;
  }

  return true;
}

void KSAdminApp::FirstTaskRun() {
  if (!ParseCommandLineOptions())
    Shutdown(1);

  if ((geteuid() == 0) && (getuid() != 0)) {
    if (setuid(0) || setgid(0)) {
      LOG(ERROR) << "Can't setuid()/setgid() appropriately.";
      Shutdown(1);
    }
  }

  PrintRequestedInfo();

  if (!register_ticket_ && !delete_ticket_ && !install_updates_ &&
      !list_updates_) {
    Shutdown(0);
  }

  if (register_ticket_) {
    Register(base::BindOnce(&KSAdminApp::Shutdown, this));
  } else if (install_updates_ || list_updates_) {
    CheckForUpdates(base::BindOnce(&KSAdminApp::Shutdown, this));
  } else if (!register_ticket_ && !delete_ticket_ && !app_id_.empty()) {
    if (NewTicketValuesSpecified()) {
      UpdateTicket();
      if (!(install_updates_ || list_updates_)) {
        Shutdown(0);
      }
    }
  } else if (delete_ticket_) {
    // Deleting tickets is not supported, as this is handled by the updater.
    Shutdown(0);
  }
}

scoped_refptr<App> MakeKSAdminApp(int argc, char* argv[]) {
  return base::MakeRefCounted<KSAdminApp>(argc, argv);
}

}  // namespace

int KSAdminAppMain(int argc, char* argv[]) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  updater::InitLogging(GetUpdaterScope(), FILE_PATH_LITERAL("ks_admin.log"));

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return MakeKSAdminApp(argc, argv)->Run();
}

}  // namespace updater
