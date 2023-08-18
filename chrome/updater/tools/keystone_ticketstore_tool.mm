// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <iostream>

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/mac/setup/ks_tickets.h"

@interface KSTicket (TestingTool)
@end

@implementation KSTicket (TestingTool)
- (instancetype)initWithAppId:(NSString*)appId
                      version:(NSString*)version
                  versionPath:(NSString*)versionPath
                   versionKey:(NSString*)versionKey
                          ecp:(NSString*)ecp
                          tag:(NSString*)tag
                      tagPath:(NSString*)tagPath
                       tagKey:(NSString*)tagKey
                    brandCode:(NSString*)brandCode
                    brandPath:(NSString*)brandPath
                     brandKey:(NSString*)brandKey
                       cohort:(NSString*)cohort
                   cohortHint:(NSString*)cohortHint
                   cohortName:(NSString*)cohortName
                 creationDate:(NSDate*)creationData {
  if ((self = [super init])) {
    productID_ = appId;
    version_ = version;
    if (ecp.length) {
      existenceChecker_ = [[KSPathExistenceChecker alloc]
          initWithFilePath:base::apple::NSStringToFilePath(ecp)];
    }
    tag_ = tag;
    if (tagPath.length) {
      tagPath_ = tagPath;
    }
    if (tagKey.length) {
      tagKey_ = tagKey;
    }
    brandCode_ = brandCode;
    if (brandPath.length) {
      brandPath_ = brandPath;
    }
    if (brandKey.length) {
      brandKey_ = brandKey;
    }
    serverURL_ =
        [NSURL URLWithString:@"https://tools.google.com/service/update"];
    serverType_ = @"Omaha";
    versionPath_ = versionPath;
    versionKey_ = versionKey;
    cohort_ = cohort;
    cohortHint_ = cohortHint;
    cohortName_ = cohortName;
    creationDate_ = creationData;
    ticketVersion_ = 1;
  }
  return self;
}
@end

namespace updater {
namespace {
constexpr char kPlistPathSwtich[] = "plist";
constexpr char kStorePathSwitch[] = "store";

void Usage() {
  std::cerr
      << "Usage:" << std::endl
      << "    Read Keystone ticket store: " << std::endl
      << "        keystone_ticket_tool --store=<path/to/Keystone.ticketstore>"
      << std::endl
      << std::endl
      << "    Convert plist ticket store to Keystone ticket store: "
      << std::endl
      << "        keystone_ticket_tool --plist=<path/to/ticketstore.plist> "
      << "--store=<path/to/Keystone.ticketstore>" << std::endl
      << std::endl;
}

int ReadTicketStore(const base::FilePath& path) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store =
        [KSTicketStore readStoreWithPath:base::apple::FilePathToNSString(path)];
    for (NSString* key in store) {
      std::cout << "------ Key " << base::SysNSStringToUTF8(key) << std::endl
                << base::SysNSStringToUTF8(
                       [[store objectForKey:key] description])
                << std::endl
                << std::endl;
    }
  }
  return 0;
}

NSDictionary<NSString*, KSTicket*>* ReadPlistTicketStore(
    const base::FilePath& input) {
  NSError* error = nil;
  NSDictionary<NSString*, NSDictionary<NSString*, id>*>* tickets_data =
      [NSDictionary
          dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(input)
                                error:&error];
  if (error) {
    std::cerr << "Error read store: "
              << base::SysNSStringToUTF8(error.description) << std::endl;
    return nil;
  }

  NSMutableDictionary* tickets = [NSMutableDictionary dictionary];
  [tickets_data
      enumerateKeysAndObjectsUsingBlock:^(id key, id ticket_data, BOOL* stop) {
        tickets[key] =
            [[KSTicket alloc] initWithAppId:key
                                    version:ticket_data[@"Version"]
                                versionPath:ticket_data[@"VersionPath"]
                                 versionKey:ticket_data[@"VersionKey"]
                                        ecp:ticket_data[@"ExistenceChecker"]
                                        tag:ticket_data[@"Tag"]
                                    tagPath:ticket_data[@"TagPath"]
                                     tagKey:ticket_data[@"TagKey"]
                                  brandCode:ticket_data[@"BrandCode"]
                                  brandPath:ticket_data[@"BrandPath"]
                                   brandKey:ticket_data[@"BrandKey"]
                                     cohort:ticket_data[@"Cohort"]
                                 cohortHint:ticket_data[@"CohortHint"]
                                 cohortName:ticket_data[@"CohortName"]
                               creationDate:ticket_data[@"CreationDate"]];
      }];
  return tickets;
}

int ConvertTicketStore(const base::FilePath& input,
                       const base::FilePath& output) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store = ReadPlistTicketStore(input);
    if (!store) {
      std::cerr << "Failed to read input ticket store, is it a valid plist?"
                << std::endl;
      return 1;
    }

    NSError* error;
    NSData* storeData = [NSKeyedArchiver archivedDataWithRootObject:store
                                              requiringSecureCoding:YES
                                                              error:&error];
    if (!storeData) {
      std::cerr << "Input ticket store data is invalid: "
                << base::SysNSStringToUTF8(error.description) << std::endl;
      return 1;
    }

    if (![storeData writeToFile:base::apple::FilePathToNSString(output)
                        options:NSDataWritingAtomic
                          error:&error]) {
      std::cerr << "Failed to write output: "
                << base::SysNSStringToUTF8(error.description) << std::endl;
      return 1;
    }
    return 0;
  }
}

}  // namespace
}  // namespace updater

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(updater::kStorePathSwitch)) {
    updater::Usage();
    return 1;
  }

  if (command_line->HasSwitch(updater::kPlistPathSwtich)) {
    return updater::ConvertTicketStore(
        command_line->GetSwitchValuePath(updater::kPlistPathSwtich),
        command_line->GetSwitchValuePath(updater::kStorePathSwitch));
  } else {
    return updater::ReadTicketStore(
        command_line->GetSwitchValuePath(updater::kStorePathSwitch));
  }
}
