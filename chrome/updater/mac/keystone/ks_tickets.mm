// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/keystone/ks_tickets.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"

@interface KSPathExistenceChecker : NSObject <NSSecureCoding> {
 @private
  NSString* path_;
}
@end

@interface KSTicket : NSObject <NSSecureCoding>
@end

@implementation KSTicketStore

+ (nullable NSDictionary*)readStoreWithPath:(nonnull NSString*)path {
  if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
    VLOG(0) << "Ticket store does not exist at "
            << base::SysNSStringToUTF8(path);
    return [NSDictionary dictionary];
  }
  NSError* readError = nil;
  NSData* storeData = [NSData dataWithContentsOfFile:path
                                             options:0  // Use normal IO
                                               error:&readError];
  if (!storeData) {
    VLOG(0) << "Failed to decode ticket store at "
            << base::SysNSStringToUTF8(path) << ": " << readError;
    return nil;
  }
  if (![storeData length])
    return [NSDictionary dictionary];
  NSDictionary* store = nil;
  @try {  // Unarchiver can throw
    NSKeyedUnarchiver* unpacker =
        [[NSKeyedUnarchiver alloc] initForReadingWithData:storeData];
    unpacker.requiresSecureCoding = YES;
    NSSet* classes =
        [NSSet setWithObjects:[NSDictionary class], [KSTicket class],
                              [KSPathExistenceChecker class], [NSArray class],
                              [NSURL class], nil];
    store = [unpacker decodeObjectOfClasses:classes
                                     forKey:NSKeyedArchiveRootObjectKey];
  } @catch (id e) {
    VLOG(0) << base::SysNSStringToUTF8(
        [NSString stringWithFormat:@"Ticket exception %@", e]);
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

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)dealloc {
  [path_ release];
  [super dealloc];
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    @try {
      path_ = [[coder decodeObjectOfClass:[NSString class]
                                   forKey:@"path"] retain];
    } @catch (id e) {
      VLOG(0) << base::SysNSStringToUTF8(
          [NSString stringWithFormat:@"Coder exception %@", e]);
      return nil;
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  NOTREACHED() << "KSPathExistenceChecker::encodeWithCoder not implemented.";
}

- (NSString*)description {
  // Formatting must stay the same in ksadmin output.
  return [NSString
      stringWithFormat:@"<%@:0x222222222222 path=%@>", [self class], path_];
}

@end

NSString* const kKSTicketCohortKey = @"Cohort";
NSString* const kKSTicketCohortHintKey = @"CohortHint";
NSString* const kKSTicketCohortNameKey = @"CohortName";

@implementation KSTicket {
 @private
  NSString* productID_;
  NSString* version_;
  KSPathExistenceChecker* existenceChecker_;
  NSURL* serverURL_;
  NSDate* creationDate_;
  NSString* serverType_;
  NSString* tag_;
  NSString* tagPath_;
  NSString* tagKey_;
  NSString* brandPath_;
  NSString* brandKey_;
  NSString* versionPath_;
  NSString* versionKey_;
  NSString* cohort_;
  NSString* cohortHint_;
  NSString* cohortName_;
  int32_t ticketVersion_;
}

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
                                         forKey:@"server_url"];
  if (!serverURL)
    return nil;
  if ([serverURL isKindOfClass:[NSString class]]) {
    return [NSURL URLWithString:serverURL];  // May throw
  }
  return (NSURL*)serverURL;
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    @try {
      productID_ = [[coder decodeObjectOfClass:[NSString class]
                                        forKey:@"product_id"] retain];
      version_ = [[coder decodeObjectOfClass:[NSString class]
                                      forKey:@"version"] retain];
      existenceChecker_ =
          [[coder decodeObjectOfClass:[KSPathExistenceChecker class]
                               forKey:@"existence_checker"] retain];
      serverURL_ = [[self decodeServerURL:coder] retain];
      creationDate_ = [[coder decodeObjectOfClass:[NSDate class]
                                           forKey:@"creation_date"] retain];
      serverType_ = [[coder decodeObjectOfClass:[NSString class]
                                         forKey:@"serverType"] retain];
      tag_ = [[coder decodeObjectOfClass:[NSString class]
                                  forKey:@"tag"] retain];
      tagPath_ = [[coder decodeObjectOfClass:[NSString class]
                                      forKey:@"tagPath"] retain];
      tagKey_ = [[coder decodeObjectOfClass:[NSString class]
                                     forKey:@"tagKey"] retain];
      brandPath_ = [[coder decodeObjectOfClass:[NSString class]
                                        forKey:@"brandPath"] retain];
      brandKey_ = [[coder decodeObjectOfClass:[NSString class]
                                       forKey:@"brandKey"] retain];
      versionPath_ = [[coder decodeObjectOfClass:[NSString class]
                                          forKey:@"versionPath"] retain];
      versionKey_ = [[coder decodeObjectOfClass:[NSString class]
                                         forKey:@"versionKey"] retain];
      cohort_ = [[coder decodeObjectOfClass:[NSString class]
                                     forKey:kKSTicketCohortKey] retain];
      cohortHint_ = [[coder decodeObjectOfClass:[NSString class]
                                         forKey:kKSTicketCohortHintKey] retain];
      cohortName_ = [[coder decodeObjectOfClass:[NSString class]
                                         forKey:kKSTicketCohortNameKey] retain];
      ticketVersion_ = [coder decodeInt32ForKey:@"ticketVersion"];
    } @catch (id e) {
      VLOG(0) << base::SysNSStringToUTF8(
          [NSString stringWithFormat:@"Coder exception %@", e]);
      return nil;
    }
  }
  return self;
}

- (void)dealloc {
  [productID_ release];
  [version_ release];
  [existenceChecker_ release];
  [serverURL_ release];
  [creationDate_ release];
  [serverType_ release];
  [tag_ release];
  [tagPath_ release];
  [tagKey_ release];
  [brandPath_ release];
  [brandKey_ release];
  [versionPath_ release];
  [versionKey_ release];
  [cohort_ release];
  [cohortHint_ release];
  [cohortName_ release];

  [super dealloc];
}

- (void)encodeWithCoder:(NSCoder*)coder {
  NOTREACHED() << "KSTicket::encodeWithCoder not implemented.";
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
  NSDateFormatter* dateFormatter = [[[NSDateFormatter alloc] init] autorelease];
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

@end
