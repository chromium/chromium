// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/setup/ks_tickets.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"

NSString* const kCRUTicketBrandKey = @"KSBrandID";
NSString* const kCRUTicketTagKey = @"KSChannelID";

@interface KSLaunchServicesExistenceChecker : NSObject <NSSecureCoding>
@property(nonnull, readonly) NSString* bundle_id;
@end

@interface KSSpotlightExistenceChecker : NSObject <NSSecureCoding>
@property(nonnull, readonly) NSString* query;
@end

@implementation KSTicketStore

+ (nullable NSDictionary<NSString*, KSTicket*>*)readStoreWithPath:
    (nonnull NSString*)path {
  if (![NSFileManager.defaultManager fileExistsAtPath:path]) {
    VLOG(0) << "Ticket store does not exist at "
            << base::SysNSStringToUTF8(path);
    return [NSDictionary dictionary];
  }

  NSError* error = nil;
  NSData* storeData = [NSData dataWithContentsOfFile:path
                                             options:0  // Use normal IO
                                               error:&error];
  if (!storeData) {
    VLOG(0) << "Failed to load ticket store at "
            << base::SysNSStringToUTF8(path) << ": " << error;
    return nil;
  }
  if (!storeData.length) {
    return [NSDictionary dictionary];
  }

  NSDictionary* store = nil;
  NSKeyedUnarchiver* unpacker =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:storeData error:&error];
  if (!unpacker) {
    VLOG(0) << base::SysNSStringToUTF8(
        [NSString stringWithFormat:@"Ticket error %@", error]);
    return nil;
  }
  unpacker.requiresSecureCoding = YES;
  NSSet* classes = [NSSet
      setWithObjects:[NSDictionary class], [KSTicket class],
                     [KSPathExistenceChecker class],
                     [KSLaunchServicesExistenceChecker class],
                     [KSSpotlightExistenceChecker class], [NSArray class],
                     [NSSet class], [NSURL class], [NSString class], nil];
  store = [unpacker decodeObjectOfClasses:classes
                                   forKey:NSKeyedArchiveRootObjectKey];
  [unpacker finishDecoding];
  if (unpacker.error) {
    VLOG(0) << "Error unpacking ticket store: " << unpacker.error;
    return nil;
  }
  if (!store || ![store isKindOfClass:[NSDictionary class]]) {
    VLOG(0) << "Ticket store is not a dictionary.";
    return nil;
  }
  return store;
}

@end

@implementation KSPathExistenceChecker

@synthesize path = path_;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    path_ = [coder decodeObjectOfClass:[NSString class] forKey:@"path"];
  }
  return self;
}

- (instancetype)initWithFilePath:(const base::FilePath&)filePath {
  if ((self = [super init])) {
    path_ = base::apple::FilePathToNSString(filePath);
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:path_ forKey:@"path"];
}

- (NSString*)description {
  // Formatting must stay the same in ksadmin output.
  return [NSString
      stringWithFormat:@"<%@:0x222222222222 path=%@>", [self class], path_];
}

@end

@implementation KSLaunchServicesExistenceChecker

@synthesize bundle_id = bundle_id_;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    bundle_id_ = [coder decodeObjectOfClass:[NSString class]
                                     forKey:@"bundle_id"];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  NOTREACHED_IN_MIGRATION();
}

- (NSString*)description {
  // Formatting must stay the same in ksadmin output.
  return [NSString stringWithFormat:@"<%@:0x222222222222 bundle_id=%@>",
                                    [self class], bundle_id_];
}

@end

@implementation KSSpotlightExistenceChecker

@synthesize query = query_;

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    query_ = [coder decodeObjectOfClass:[NSString class] forKey:@"query"];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  NOTREACHED_IN_MIGRATION();
}

- (NSString*)description {
  // Formatting must stay the same in ksadmin output.
  return [NSString
      stringWithFormat:@"<%@:0x222222222222 query=%@>", [self class], query_];
}

@end

// All these keys must be same as those from Keystone.
NSString* const kKSTicketBrandKeyKey = @"brandKey";
NSString* const kKSTicketBrandPathKey = @"brandPath";
NSString* const kKSTicketCohortKey = @"Cohort";
NSString* const kKSTicketCohortHintKey = @"CohortHint";
NSString* const kKSTicketCohortNameKey = @"CohortName";
NSString* const kKSTicketCreationDateKey = @"creation_date";
NSString* const kKSTicketExistenceCheckerKey = @"existence_checker";
NSString* const kKSTicketProductIDKey = @"product_id";
NSString* const kKSTicketServerTypeKey = @"serverType";
NSString* const kKSTicketServerURLKey = @"server_url";
NSString* const kKSTicketTagKey = @"tag";
NSString* const kKSTicketTagKeyKey = @"tagKey";
NSString* const kKSTicketTagPathKey = @"tagPath";
NSString* const kKSTicketTicketVersionKey = @"ticketVersion";
NSString* const kKSTicketVersionKey = @"version";
NSString* const kKSTicketVersionPathKey = @"versionPath";
NSString* const kKSTicketVersionKeyKey = @"versionKey";

@implementation KSTicket

@synthesize productID = productID_;
@synthesize version = version_;
@synthesize existenceChecker = existenceChecker_;
@synthesize serverURL = serverURL_;
@synthesize serverType = serverType_;
@synthesize creationDate = creationDate_;
@synthesize tag = tag_;
@synthesize tagPath = tagPath_;
@synthesize tagKey = tagKey_;
@synthesize brandPath = brandPath_;
@synthesize brandKey = brandKey_;
@synthesize versionPath = versionPath_;
@synthesize versionKey = versionKey_;
@synthesize cohort = cohort_;
@synthesize cohortHint = cohortHint_;
@synthesize cohortName = cohortName_;
@synthesize ticketVersion = ticketVersion_;

+ (BOOL)supportsSecureCoding {
  return YES;
}

// Tries to obtain the server URL which may be NSURL or NSString object.
// Verifies the read objects and guarantees that the returned object is NSURL.
// The method may throw.
- (NSURL*)decodeServerURL:(NSCoder*)decoder {
  id serverURL = [decoder decodeObjectOfClasses:[NSSet setWithArray:@[
                            [NSString class],
                            [NSURL class],
                          ]]
                                         forKey:kKSTicketServerURLKey];
  if (!serverURL) {
    return nil;
  }
  if ([serverURL isKindOfClass:[NSString class]]) {
    return [NSURL URLWithString:serverURL];  // May throw
  }
  return (NSURL*)serverURL;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    productID_ = [coder decodeObjectOfClass:[NSString class]
                                     forKey:kKSTicketProductIDKey];
    version_ = [coder decodeObjectOfClass:[NSString class]
                                   forKey:kKSTicketVersionKey];
    if ([[coder decodeObjectForKey:kKSTicketExistenceCheckerKey]
            isKindOfClass:[KSPathExistenceChecker class]]) {
      existenceChecker_ =
          [coder decodeObjectOfClass:[KSPathExistenceChecker class]
                              forKey:kKSTicketExistenceCheckerKey];
    }
    serverURL_ = [self decodeServerURL:coder];
    creationDate_ = [coder decodeObjectOfClass:[NSDate class]
                                        forKey:kKSTicketCreationDateKey];
    serverType_ = [coder decodeObjectOfClass:[NSString class]
                                      forKey:kKSTicketServerTypeKey];
    tag_ = [coder decodeObjectOfClass:[NSString class] forKey:kKSTicketTagKey];
    tagPath_ = [coder decodeObjectOfClass:[NSString class]
                                   forKey:kKSTicketTagPathKey];
    tagKey_ = [coder decodeObjectOfClass:[NSString class]
                                  forKey:kKSTicketTagKeyKey];
    brandPath_ = [coder decodeObjectOfClass:[NSString class]
                                     forKey:kKSTicketBrandPathKey];
    brandKey_ = [coder decodeObjectOfClass:[NSString class]
                                    forKey:kKSTicketBrandKeyKey];
    versionPath_ = [coder decodeObjectOfClass:[NSString class]
                                       forKey:kKSTicketVersionPathKey];
    versionKey_ = [coder decodeObjectOfClass:[NSString class]
                                      forKey:kKSTicketVersionKeyKey];
    cohort_ = [coder decodeObjectOfClass:[NSString class]
                                  forKey:kKSTicketCohortKey];
    cohortHint_ = [coder decodeObjectOfClass:[NSString class]
                                      forKey:kKSTicketCohortHintKey];
    cohortName_ = [coder decodeObjectOfClass:[NSString class]
                                      forKey:kKSTicketCohortNameKey];
    ticketVersion_ = [coder decodeInt32ForKey:kKSTicketTicketVersionKey];
  }
  return self;
}

- (instancetype)initWithAppId:(NSString*)appId
                      version:(NSString*)version
                          ecp:(const base::FilePath&)ecp
                          tag:(NSString*)tag
                    brandCode:(NSString*)brandCode
                    brandPath:(const base::FilePath&)brandPath {
  if ((self = [super init])) {
    productID_ = appId;
    version_ = version;
    if (!ecp.empty()) {
      existenceChecker_ = [[KSPathExistenceChecker alloc] initWithFilePath:ecp];

      tagPath_ =
          [NSString stringWithFormat:@"%@/Contents/Info.plist",
                                     base::apple::FilePathToNSString(ecp)];
      tagKey_ = kCRUTicketTagKey;
    }
    tag_ = tag;

    brandCode_ = brandCode;
    if (!brandPath.empty()) {
      brandPath_ = base::apple::FilePathToNSString(brandPath);
      brandKey_ = kCRUTicketBrandKey;
    }
    serverURL_ =
        [NSURL URLWithString:@"https://tools.google.com/service/update2"];
    serverType_ = @"Omaha";
    ticketVersion_ = 1;
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:productID_ forKey:kKSTicketProductIDKey];
  [coder encodeObject:version_ forKey:kKSTicketVersionKey];
  [coder encodeObject:existenceChecker_ forKey:kKSTicketExistenceCheckerKey];
  [coder encodeObject:serverURL_ forKey:kKSTicketServerURLKey];
  [coder encodeObject:creationDate_ forKey:kKSTicketCreationDateKey];
  if (serverType_.length) {
    [coder encodeObject:serverType_ forKey:kKSTicketServerTypeKey];
  }
  if (tag_.length) {
    [coder encodeObject:tag_ forKey:kKSTicketTagKey];
  }
  if (tagPath_.length) {
    [coder encodeObject:tagPath_ forKey:kKSTicketTagPathKey];
  }
  if (tagKey_.length) {
    [coder encodeObject:tagKey_ forKey:kKSTicketTagKeyKey];
  }
  if (brandPath_.length) {
    [coder encodeObject:brandPath_ forKey:kKSTicketBrandPathKey];
  }
  if (brandKey_.length) {
    [coder encodeObject:brandKey_ forKey:kKSTicketBrandKeyKey];
  }
  if (versionPath_.length) {
    [coder encodeObject:versionPath_ forKey:kKSTicketVersionPathKey];
  }
  if (versionKey_.length) {
    [coder encodeObject:versionKey_ forKey:kKSTicketVersionKeyKey];
  }
  if (cohort_.length) {
    [coder encodeObject:cohort_ forKey:kKSTicketCohortKey];
  }
  if (cohortHint_.length) {
    [coder encodeObject:cohortHint_ forKey:kKSTicketCohortHintKey];
  }
  if (cohortName_.length) {
    [coder encodeObject:cohortName_ forKey:kKSTicketCohortNameKey];
  }
  [coder encodeInt32:ticketVersion_ forKey:kKSTicketTicketVersionKey];
}

- (NSUInteger)hash {
  return [productID_ hash] + [version_ hash] + [existenceChecker_ hash] +
         [serverURL_ hash] + [creationDate_ hash];
}

- (NSString*)description {
  // Keep the description stable.  Clients depend on the output formatting
  // as "fieldname=value" without any additional quoting. We cannot use
  // KSDescription() here because of these legacy formatting restrictions. In
  // particular, ksadmin output must not be substantially changed.
  NSString* serverTypeString = @"";
  if (serverType_) {
    serverTypeString =
        [NSString stringWithFormat:@"\n\tserverType=%@", serverType_];
  }
  NSString* tagString = @"";
  if (tag_) {
    tagString = [NSString stringWithFormat:@"\n\ttag=%@", tag_];
  }
  NSString* tagPathString = @"";
  if (tagPath_ && tagKey_) {
    tagPathString = [NSString
        stringWithFormat:@"\n\ttagPath=%@\n\ttagKey=%@", tagPath_, tagKey_];
  }
  NSString* brandPathString = @"";
  if (brandPath_ && brandKey_) {
    brandPathString =
        [NSString stringWithFormat:@"\n\tbrandPath=%@\n\tbrandKey=%@",
                                   brandPath_, brandKey_];
  }
  NSString* versionPathString = @"";
  if (versionPath_ && versionKey_) {
    versionPathString =
        [NSString stringWithFormat:@"\n\tversionPath=%@\n\tversionKey=%@",
                                   versionPath_, versionKey_];
  }
  NSString* cohortString = @"";
  if ([cohort_ length]) {
    cohortString = [NSString stringWithFormat:@"\n\tcohort=%@", cohort_];
    if ([cohortName_ length]) {
      cohortString = [cohortString
          stringByAppendingFormat:@"\n\tcohortName=%@", cohortName_];
    }
  }
  NSString* cohortHintString = @"";
  if ([cohortHint_ length]) {
    cohortHintString =
        [NSString stringWithFormat:@"\n\tcohortHint=%@", cohortHint_];
  }
  NSString* ticketVersionString =
      [NSString stringWithFormat:@"\n\tticketVersion=%d", ticketVersion_];
  // Dates used to be parsed and stored as GMT and printed in GMT. That
  // changed in 10.7 to be GMT with timezone information, so use a custom
  // description string that matches our old output.
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
  [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
  NSString* gmtDate = [dateFormatter stringFromDate:creationDate_];
  return [NSString
      stringWithFormat:@"<%@:0x222222222222\n\tproductID=%@\n\tversion=%@\n\t"
                       @"xc=%@%@\n\turl=%@\n\tcreationDate=%@%@%@%@%@%@%@%@\n>",
                       [self class], productID_, version_, existenceChecker_,
                       serverTypeString, serverURL_, gmtDate, tagString,
                       tagPathString, brandPathString, versionPathString,
                       cohortString, cohortHintString, ticketVersionString];
}

- (NSString*)readExternalPropertyAtPath:(NSString*)path withKey:(NSString*)key {
  // Standardize (expands tilde, symlink resolve, etc.)
  NSString* fullPath = [path stringByStandardizingPath];

  if (!fullPath.length || !key.length) {
    return nil;
  }

  NSData* plistData = [NSData dataWithContentsOfFile:fullPath];
  if (!plistData.length) {
    LOG(ERROR) << "Failed to read external property from file: "
               << base::SysNSStringToUTF8(path);
    return nil;
  }

  id plistContent =
      [NSPropertyListSerialization propertyListWithData:plistData
                                                options:NSPropertyListImmutable
                                                 format:nil
                                                  error:nil];
  if (!plistContent || ![plistContent isKindOfClass:[NSDictionary class]]) {
    return nil;
  }

  id value = [plistContent objectForKey:key];
  if (![value isKindOfClass:[NSString class]]) {
    return nil;
  }

  return (NSString*)value;
}

- (NSString*)determineTag {
  NSString* externalTag = [self readExternalPropertyAtPath:tagPath_
                                                   withKey:tagKey_];
  return externalTag ? externalTag : tag_;
}

- (NSString*)determineBrand {
  return [self readExternalPropertyAtPath:brandPath_ withKey:brandKey_];
}

- (NSString*)determineVersion {
  NSString* externalVersion = [self readExternalPropertyAtPath:versionPath_
                                                       withKey:versionKey_];
  return externalVersion ? externalVersion : version_;
}

@end
