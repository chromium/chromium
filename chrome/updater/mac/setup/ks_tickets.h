// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_
#define CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_

#import <Foundation/Foundation.h>

extern NSString* _Nonnull const kCRUTicketBrandKey;
extern NSString* _Nonnull const kCRUTicketTagKey;

namespace base {
class FilePath;
}

@interface KSPathExistenceChecker : NSObject <NSSecureCoding>
@property(nonnull, readonly) NSString* path;

- (nullable instancetype)initWithFilePath:(const base::FilePath&)filePath;
@end

@interface KSTicket : NSObject <NSSecureCoding> {
  NSString* __strong productID_;
  NSString* __strong version_;
  NSString* __strong brandCode_;
  KSPathExistenceChecker* __strong existenceChecker_;
  NSURL* __strong serverURL_;
  NSString* __strong serverType_;
  NSDate* __strong creationDate_;
  NSString* __strong tag_;
  NSString* __strong tagPath_;
  NSString* __strong tagKey_;
  NSString* __strong brandPath_;
  NSString* __strong brandKey_;
  NSString* __strong versionPath_;
  NSString* __strong versionKey_;
  NSString* __strong cohort_;
  NSString* __strong cohortHint_;
  NSString* __strong cohortName_;
  int32_t ticketVersion_;
}

@property(nonnull, nonatomic, readonly) NSString* productID;
@property(nullable, nonatomic, readonly)
    KSPathExistenceChecker* existenceChecker;
@property(nullable, nonatomic, readonly) NSURL* serverURL;
@property(nonnull, nonatomic, readonly) NSDate* creationDate;
@property(nullable, nonatomic, readonly) NSString* serverType;
@property(nullable, nonatomic, readonly) NSString* tag;
@property(nullable, nonatomic, readonly) NSString* tagPath;
@property(nullable, nonatomic, readonly) NSString* tagKey;
@property(nullable, nonatomic, readonly) NSString* brandPath;
@property(nullable, nonatomic, readonly) NSString* brandKey;
@property(nullable, nonatomic, readonly) NSString* version;
@property(nullable, nonatomic, readonly) NSString* versionPath;
@property(nullable, nonatomic, readonly) NSString* versionKey;
@property(nullable, nonatomic, readonly) NSString* cohort;
@property(nullable, nonatomic, readonly) NSString* cohortHint;
@property(nullable, nonatomic, readonly) NSString* cohortName;
@property int32_t ticketVersion;

// Values that are sent as the attributes in the update check request.
- (nullable NSString*)determineTag;      // ap
- (nullable NSString*)determineBrand;    // brand
- (nullable NSString*)determineVersion;  // version

- (nonnull instancetype)initWithAppId:(nonnull NSString*)appId
                              version:(nullable const NSString*)version
                                  ecp:(const base::FilePath&)ecp
                                  tag:(nullable NSString*)tag
                            brandCode:(nullable NSString*)brandCode
                            brandPath:(const base::FilePath&)brandPath;

@end

// KSTicketStore holds a class method for reading an NSDictionary of NSString
// to KSTickets.
@interface KSTicketStore : NSObject

+ (nullable NSDictionary<NSString*, KSTicket*>*)readStoreWithPath:
    (nonnull NSString*)path;

@end

#endif  // CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_
