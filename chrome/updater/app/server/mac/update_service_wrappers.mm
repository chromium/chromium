// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/update_service_wrappers.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/app/server/mac/service_protocol.h"

static NSString* const kCRUUpdateStateAppId = @"updateStateAppId";
static NSString* const kCRUUpdateStateState = @"updateStateState";
static NSString* const kCRUUpdateStateVersion = @"updateStateVersion";
static NSString* const kCRUUpdateStateDownloadedBytes =
    @"updateStateDownloadedBytes";
static NSString* const kCRUUpdateStateTotalBytes = @"updateStateTotalBytes";
static NSString* const kCRUUpdateStateInstallProgress =
    @"updateStateInstallProgress";
static NSString* const kCRUUpdateStateErrorCategory =
    @"updateStateErrorCategory";
static NSString* const kCRUUpdateStateErrorCode = @"updateStateErrorCode";
static NSString* const kCRUUpdateStateExtraCode = @"updateStateExtraCode";

static NSString* const kCRUPriority = @"priority";
static NSString* const kCRUErrorCategory = @"errorCategory";

using StateChangeCallback =
    base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>;

@implementation CRUUpdateStateObserver {
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
}

@synthesize callback = _callback;

- (instancetype)initWithRepeatingCallback:(StateChangeCallback)callback
                           callbackRunner:
                               (scoped_refptr<base::SequencedTaskRunner>)
                                   callbackRunner {
  if (self = [super init]) {
    _callback = callback;
    _callbackRunner = callbackRunner;
  }
  return self;
}

- (void)observeUpdateState:(CRUUpdateStateWrapper* _Nonnull)updateState {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindRepeating(_callback, [updateState updateState]));
}

@end

@implementation CRUUpdateStateWrapper

@synthesize updateState = _updateState;

@synthesize appId = _appId;
@synthesize state = _state;
@synthesize version = _version;
@synthesize errorCategory = _errorCategory;

// Designated initializer.
- (instancetype)initWithAppId:(NSString*)appId
                        state:(CRUUpdateStateStateWrapper*)state
                      version:(NSString*)version
              downloadedBytes:(int64_t)downloadedBytes
                   totalBytes:(int64_t)totalBytes
              installProgress:(int)installProgress
                errorCategory:(CRUErrorCategoryWrapper*)errorCategory
                    errorCode:(int)errorCode
                    extraCode:(int)extraCode {
  if (self = [super init]) {
    _appId = [appId copy];
    _state = [state retain];
    _version = [version copy];
    _errorCategory = [errorCategory retain];

    _updateState.app_id = base::SysNSStringToUTF8(appId);
    _updateState.state = state.updateStateState;
    _updateState.next_version = base::Version(base::SysNSStringToUTF8(version));
    _updateState.downloaded_bytes = downloadedBytes;
    _updateState.total_bytes = totalBytes;
    _updateState.install_progress = installProgress;
    _updateState.error_category = errorCategory.errorCategory;
    _updateState.error_code = errorCode;
    _updateState.extra_code1 = extraCode;
  }
  return self;
}

- (void)dealloc {
  [_appId release];
  [_state release];
  [_version release];
  [_errorCategory release];

  [super dealloc];
}

- (int64_t)downloadedBytes {
  return self.updateState.downloaded_bytes;
}

- (int64_t)totalBytes {
  return self.updateState.total_bytes;
}

- (int)installProgress {
  return self.updateState.install_progress;
}

- (int)errorCode {
  return self.updateState.error_code;
}

- (int)extraCode {
  return self.updateState.extra_code1;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSString* appId = [aDecoder decodeObjectOfClass:[NSString class]
                                           forKey:kCRUUpdateStateAppId];
  CRUUpdateStateStateWrapper* state =
      [aDecoder decodeObjectOfClass:[CRUUpdateStateStateWrapper class]
                             forKey:kCRUUpdateStateState];
  NSString* version = [aDecoder decodeObjectOfClass:[NSString class]
                                             forKey:kCRUUpdateStateVersion];
  int64_t downloadedBytes =
      [aDecoder decodeInt64ForKey:kCRUUpdateStateDownloadedBytes];
  int64_t totalBytes = [aDecoder decodeInt64ForKey:kCRUUpdateStateTotalBytes];
  int installProgress =
      [aDecoder decodeIntForKey:kCRUUpdateStateInstallProgress];
  CRUErrorCategoryWrapper* errorCategory =
      [aDecoder decodeObjectOfClass:[CRUErrorCategoryWrapper class]
                             forKey:kCRUUpdateStateErrorCategory];
  int errorCode = [aDecoder decodeIntForKey:kCRUUpdateStateErrorCode];
  int extraCode = [aDecoder decodeIntForKey:kCRUUpdateStateExtraCode];

  return [self initWithAppId:appId
                       state:state
                     version:version
             downloadedBytes:downloadedBytes
                  totalBytes:totalBytes
             installProgress:installProgress
               errorCategory:errorCategory
                   errorCode:errorCode
                   extraCode:extraCode];
}

- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeObject:forKey:)]);
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  DCHECK([coder respondsToSelector:@selector(encodeInt64:forKey:)]);
  [coder encodeObject:self.appId forKey:kCRUUpdateStateAppId];
  [coder encodeObject:self.state forKey:kCRUUpdateStateState];
  [coder encodeObject:self.version forKey:kCRUUpdateStateVersion];
  [coder encodeInt64:self.downloadedBytes
              forKey:kCRUUpdateStateDownloadedBytes];
  [coder encodeInt64:self.totalBytes forKey:kCRUUpdateStateTotalBytes];
  [coder encodeInt:self.installProgress forKey:kCRUUpdateStateInstallProgress];
  [coder encodeObject:self.errorCategory forKey:kCRUUpdateStateErrorCategory];
  [coder encodeInt:self.errorCode forKey:kCRUUpdateStateErrorCode];
  [coder encodeInt:self.extraCode forKey:kCRUUpdateStateExtraCode];
}

@end

@implementation CRUUpdateStateStateWrapper

@synthesize updateStateState = _updateStateState;

// Wrapper for updater::UpdateService::UpdateState::State
typedef NS_ENUM(NSInteger, CRUUpdateStateStateEnum) {
  kCRUUpdateStateStateUnknown = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kUnknown),
  kCRUUpdateStateStateNotStarted = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kNotStarted),
  kCRUUpdateStateStateCheckingForUpdates = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kCheckingForUpdates),
  kCRUUPdateStateStateUpdateAvailable = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kUpdateAvailable),
  kCRUUpdateStateStateDownloading = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kDownloading),
  kCRUUpdateStateStateInstalling = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kInstalling),
  kCRUUpdateStateStateUpdated = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kUpdated),
  kCRUUpdateStateStateNoUpdate = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kNoUpdate),
  kCRUUpdateStateStateUpdateError = static_cast<NSInteger>(
      updater::UpdateService::UpdateState::State::kUpdateError),
};

// Designated initializer.
- (instancetype)initWithUpdateStateState:
    (updater::UpdateService::UpdateState::State)updateStateState {
  if (self = [super init]) {
    _updateStateState = updateStateState;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue = [aDecoder decodeIntegerForKey:kCRUUpdateStateState];

  switch (enumValue) {
    case kCRUUpdateStateStateUnknown:
      return [self initWithUpdateStateState:updater::UpdateService::
                                                UpdateState::State::kUnknown];
    case kCRUUpdateStateStateNotStarted:
      return [self initWithUpdateStateState:
                       updater::UpdateService::UpdateState::State::kNotStarted];
    case kCRUUpdateStateStateCheckingForUpdates:
      return
          [self initWithUpdateStateState:updater::UpdateService::UpdateState::
                                             State::kCheckingForUpdates];
    case kCRUUPdateStateStateUpdateAvailable:
      return
          [self initWithUpdateStateState:updater::UpdateService::UpdateState::
                                             State::kUpdateAvailable];
    case kCRUUpdateStateStateDownloading:
      return
          [self initWithUpdateStateState:updater::UpdateService::UpdateState::
                                             State::kDownloading];
    case kCRUUpdateStateStateInstalling:
      return [self initWithUpdateStateState:
                       updater::UpdateService::UpdateState::State::kInstalling];
    case kCRUUpdateStateStateUpdated:
      return [self initWithUpdateStateState:updater::UpdateService::
                                                UpdateState::State::kUpdated];
    case kCRUUpdateStateStateNoUpdate:
      return [self initWithUpdateStateState:updater::UpdateService::
                                                UpdateState::State::kNoUpdate];
    case kCRUUpdateStateStateUpdateError:
      return
          [self initWithUpdateStateState:updater::UpdateService::UpdateState::
                                             State::kUpdateError];
    default:
      DLOG(ERROR) << "Unexpected value for CRUUpdateStateStateEnum: "
                  << enumValue;
      return nil;
  }
}

- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(self.updateStateState)
            forKey:kCRUUpdateStateState];
}

@end

@implementation CRUPriorityWrapper

@synthesize priority = _priority;

// Wrapper for updater::UpdateService::Priority
typedef NS_ENUM(NSInteger, CRUUpdatePriorityEnum) {
  kCRUUpdatePriorityUnknown =
      static_cast<NSInteger>(updater::UpdateService::Priority::kUnknown),
  kCRUUpdatePriorityBackground =
      static_cast<NSInteger>(updater::UpdateService::Priority::kBackground),
  kCRUUpdatePriorityForeground =
      static_cast<NSInteger>(updater::UpdateService::Priority::kForeground),
};

// Designated initializer.
- (instancetype)initWithPriority:(updater::UpdateService::Priority)priority {
  if (self = [super init]) {
    _priority = priority;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue = [aDecoder decodeIntegerForKey:kCRUPriority];

  switch (enumValue) {
    case kCRUUpdatePriorityUnknown:
      return [self initWithPriority:updater::UpdateService::Priority::kUnknown];
    case kCRUUpdatePriorityBackground:
      return
          [self initWithPriority:updater::UpdateService::Priority::kBackground];
    case kCRUUpdatePriorityForeground:
      return
          [self initWithPriority:updater::UpdateService::Priority::kForeground];
    default:
      DLOG(ERROR) << "Unexpected value for CRUUpdatePriorityEnum: "
                  << enumValue;
      return nil;
  }
}
- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(self.priority) forKey:kCRUPriority];
}

// Required for unit tests.
- (BOOL)isEqual:(id)object {
  if (![object isMemberOfClass:[CRUPriorityWrapper class]]) {
    return NO;
  }
  CRUPriorityWrapper* otherPriorityWrapper = object;
  return self.priority == otherPriorityWrapper.priority;
}

// Required because isEqual is overridden.
- (NSUInteger)hash {
  return static_cast<NSUInteger>(_priority);
}

@end

@implementation CRUErrorCategoryWrapper

@synthesize errorCategory = _errorCategory;

// Wrapper for updater::UpdateService::ErrorCategory
typedef NS_ENUM(NSInteger, CRUErrorCategoryEnum) {
  kCRUErrorCategoryNone =
      static_cast<NSInteger>(updater::UpdateService::ErrorCategory::kNone),
  kCRUErrorCategoryDownload =
      static_cast<NSInteger>(updater::UpdateService::ErrorCategory::kDownload),
  kCRUErrorCategoryUnpack =
      static_cast<NSInteger>(updater::UpdateService::ErrorCategory::kUnpack),
  kCRUErrorCategoryInstall =
      static_cast<NSInteger>(updater::UpdateService::ErrorCategory::kInstall),
  kCRUErrorCategoryService =
      static_cast<NSInteger>(updater::UpdateService::ErrorCategory::kService),
  kCRUErrorCategoryUpdateCheck = static_cast<NSInteger>(
      updater::UpdateService::ErrorCategory::kUpdateCheck),
};

// Designated initializer.
- (instancetype)initWithErrorCategory:
    (updater::UpdateService::ErrorCategory)errorCategory {
  if (self = [super init]) {
    _errorCategory = errorCategory;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue = [aDecoder decodeIntegerForKey:kCRUErrorCategory];

  switch (enumValue) {
    case kCRUErrorCategoryNone:
      return [self
          initWithErrorCategory:updater::UpdateService::ErrorCategory::kNone];
    case kCRUErrorCategoryDownload:
      return [self initWithErrorCategory:updater::UpdateService::ErrorCategory::
                                             kDownload];
    case kCRUErrorCategoryUnpack:
      return [self
          initWithErrorCategory:updater::UpdateService::ErrorCategory::kUnpack];
    case kCRUErrorCategoryInstall:
      return [self initWithErrorCategory:updater::UpdateService::ErrorCategory::
                                             kInstall];
    case kCRUErrorCategoryService:
      return [self initWithErrorCategory:updater::UpdateService::ErrorCategory::
                                             kService];
    case kCRUErrorCategoryUpdateCheck:
      return [self initWithErrorCategory:updater::UpdateService::ErrorCategory::
                                             kUpdateCheck];
    default:
      DLOG(ERROR) << "Unexpected value for CRUErrorCategoryEnum: " << enumValue;
      return nil;
  }
}
- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(self.errorCategory)
            forKey:kCRUErrorCategory];
}

@end
