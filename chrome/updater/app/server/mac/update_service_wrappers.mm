// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/mac/update_service_wrappers.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
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
static NSString* const kCRUPolicySameVersionUpdate = @"policySameVersionUpdate";
static NSString* const kCRUErrorCategory = @"errorCategory";

static NSString* const kCRUAppStateWrappers = @"appStateWrappers";
static NSString* const kCRUAppStateAppId = @"appStateAppId";
static NSString* const kCRUAppStateVersion = @"appStateVersion";
static NSString* const kCRUAppStateAp = @"appStateAp";
static NSString* const kCRUAppStateBrandCode = @"appStateBrandCode";
static NSString* const kCRUAppStateBrandPath = @"appStateBrandPath";
static NSString* const kCRUAppStateExistenceChecker =
    @"appStateExistenceChecker";

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

@implementation CRUPolicySameVersionUpdateWrapper

@synthesize policySameVersionUpdate = _policySameVersionUpdate;

// Wrapper for updater::UpdateService::PolicySameVersionUpdate.
typedef NS_ENUM(NSInteger, CRUUpdatePolicySameVersionUpdateEnum) {
  kCRUPolicySameVersionUpdateNotAllowed = static_cast<NSInteger>(
      updater::UpdateService::PolicySameVersionUpdate::kNotAllowed),
  kCRUPolicySameVersionUpdateAllowed = static_cast<NSInteger>(
      updater::UpdateService::PolicySameVersionUpdate::kAllowed),
};

// Designated initializer.
- (instancetype)initWithPolicySameVersionUpdate:
    (updater::UpdateService::PolicySameVersionUpdate)policySameVersionUpdate {
  if (self = [super init]) {
    _policySameVersionUpdate = policySameVersionUpdate;
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);
  NSInteger enumValue =
      [aDecoder decodeIntegerForKey:kCRUPolicySameVersionUpdate];

  switch (enumValue) {
    case kCRUPolicySameVersionUpdateNotAllowed:
      return [self
          initWithPolicySameVersionUpdate:
              updater::UpdateService::PolicySameVersionUpdate::kNotAllowed];
    case kCRUPolicySameVersionUpdateAllowed:
      return
          [self initWithPolicySameVersionUpdate:
                    updater::UpdateService::PolicySameVersionUpdate::kAllowed];
    default:
      DLOG(ERROR)
          << "Unexpected value for CRUUpdatePolicySameVersionUpdateEnum: "
          << enumValue;
      return nil;
  }
}
- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeInt:forKey:)]);
  [coder encodeInt:static_cast<NSInteger>(self.policySameVersionUpdate)
            forKey:kCRUPolicySameVersionUpdate];
}
// Required for unit tests.
- (BOOL)isEqual:(id)object {
  if (![object isMemberOfClass:[CRUPolicySameVersionUpdateWrapper class]]) {
    return NO;
  }
  CRUPolicySameVersionUpdateWrapper* otherPolicySameVersionUpdateWrapper =
      object;
  return self.policySameVersionUpdate ==
         otherPolicySameVersionUpdateWrapper.policySameVersionUpdate;
}

// Required because isEqual is overridden.
- (NSUInteger)hash {
  return static_cast<NSUInteger>(_policySameVersionUpdate);
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

@implementation CRUAppStateWrapper

@synthesize state = _state;

- (instancetype)initWithAppState:
                    (const updater::UpdateService::AppState&)appState
                  restrictedView:(bool)restrictedView {
  if (self = [super init]) {
    _state = appState;
    if (restrictedView) {
      _state.ecp = base::FilePath();
    }
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeObject:forKey:)]);

  [coder encodeObject:base::SysUTF8ToNSString(self.state.app_id)
               forKey:kCRUAppStateAppId];
  [coder encodeObject:base::SysUTF8ToNSString(self.state.version.GetString())
               forKey:kCRUAppStateVersion];
  [coder encodeObject:base::SysUTF8ToNSString(self.state.ap)
               forKey:kCRUAppStateAp];
  [coder encodeObject:base::SysUTF8ToNSString(self.state.brand_code)
               forKey:kCRUAppStateBrandCode];
  [coder encodeObject:base::mac::FilePathToNSString(self.state.brand_path)
               forKey:kCRUAppStateBrandPath];
  [coder encodeObject:base::mac::FilePathToNSString(self.state.ecp)
               forKey:kCRUAppStateExistenceChecker];
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);

  NSString* appId = [aDecoder decodeObjectOfClass:[NSString class]
                                           forKey:kCRUAppStateAppId];
  NSString* version = [aDecoder decodeObjectOfClass:[NSString class]
                                             forKey:kCRUAppStateVersion];

  NSString* ap = [aDecoder decodeObjectOfClass:[NSString class]
                                        forKey:kCRUAppStateAp];
  NSString* brandCode = [aDecoder decodeObjectOfClass:[NSString class]
                                               forKey:kCRUAppStateBrandCode];
  NSString* brandPath = [aDecoder decodeObjectOfClass:[NSString class]
                                               forKey:kCRUAppStateBrandPath];
  NSString* ecp = [aDecoder decodeObjectOfClass:[NSString class]
                                         forKey:kCRUAppStateExistenceChecker];

  updater::UpdateService::AppState appState;
  appState.app_id = base::SysNSStringToUTF8(appId);
  appState.version = base::Version(base::SysNSStringToUTF8(version));
  appState.ap = base::SysNSStringToUTF8(ap);
  appState.brand_code = base::SysNSStringToUTF8(brandCode);
  appState.brand_path = base::mac::NSStringToFilePath(brandPath);
  appState.ecp = base::mac::NSStringToFilePath(ecp);
  return [self initWithAppState:appState restrictedView:NO];
}

@end

@implementation CRUAppStatesWrapper {
  NSArray<CRUAppStateWrapper*>* _states;
}

- (instancetype)initWithAppStateWrappers:
    (NSArray<CRUAppStateWrapper*>*)appStates {
  if (self = [super init]) {
    _states = [appStates copy];
  }
  return self;
}

- (void)dealloc {
  [_states release];
  [super dealloc];
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (instancetype)initWithAppStates:
                    (const std::vector<updater::UpdateService::AppState>&)
                        appStates
                   restrictedView:(bool)restrictedView {
  NSMutableArray<CRUAppStateWrapper*>* stateWrappers = [NSMutableArray array];
  for (const auto& state : appStates) {
    [stateWrappers addObject:[[[CRUAppStateWrapper alloc]
                                 initWithAppState:state
                                   restrictedView:restrictedView] autorelease]];
  }

  return [self initWithAppStateWrappers:stateWrappers];
}

- (std::vector<updater::UpdateService::AppState>)states {
  std::vector<updater::UpdateService::AppState> appStates;
  for (CRUAppStateWrapper* wrapper in _states) {
    appStates.push_back(wrapper.state);
  }

  return appStates;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  DCHECK([coder respondsToSelector:@selector(encodeObject:forKey:)]);

  [coder encodeObject:_states forKey:kCRUAppStateWrappers];
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([aDecoder allowsKeyedCoding]);

  NSSet* objectClasses =
      [NSSet setWithObjects:[NSArray class], [CRUAppStateWrapper class], nil];
  NSArray<CRUAppStateWrapper*>* stateWrappers =
      [aDecoder decodeObjectOfClasses:objectClasses
                               forKey:kCRUAppStateWrappers];
  return [self initWithAppStateWrappers:stateWrappers];
}

@end
