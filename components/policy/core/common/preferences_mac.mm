// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/preferences_mac.h"
#include "components/policy/policy_constants.h"

// `CFPrefsManagedSource` and `_CFXPreferences` are used to determine the scope
// of a policy. A policy can be read with `copyValueForKey()` below with
// `kCFPreferencesAnyUser` will be treated as machine scope policy. Otherwise,
// it will be user scope policy.
@interface _CFXPreferences : NSObject
@end

@interface CFPrefsManagedSource : NSObject
- (instancetype)initWithDomain:(NSString*)domain
                          user:(NSString*)user
                        byHost:(BOOL)by_host
                 containerPath:(NSString*)path
         containingPreferences:(_CFXPreferences*)contain_prefs;
- (id)copyValueForKey:(NSString*)key;
@end

namespace {

_CFXPreferences* CreateCFXPrefs() {
  Class prefs_class = NSClassFromString(@"_CFXPreferences");
  if (!prefs_class) {
    return nil;
  }

  return [[prefs_class alloc] init];
}

CFPrefsManagedSource* CreateCFPrefsManagedSourceForMachine(
    CFStringRef application_id,
    _CFXPreferences* cfx_prefs) {
  if (!cfx_prefs) {
    return nil;
  }

  Class source_class = NSClassFromString(@"CFPrefsManagedSource");
  if (!source_class ||
      ![source_class
          instancesRespondToSelector:@selector
          (initWithDomain:user:byHost:containerPath:containingPreferences:)] ||
      ![source_class instancesRespondToSelector:@selector(copyValueForKey:)]) {
    return nil;
  }

  return [[source_class alloc]
             initWithDomain:base::apple::CFToNSPtrCast(application_id)
                       user:base::apple::CFToNSPtrCast(kCFPreferencesAnyUser)
                     byHost:YES
              containerPath:nil
      containingPreferences:cfx_prefs];
}

}  // namespace

class MacPreferences::PolicyScope {
 public:
  void Init(CFStringRef application_id) {
    if (!cfx_prefs_) {
      cfx_prefs_ = CreateCFXPrefs();
    }
    machine_scope_ =
        CreateCFPrefsManagedSourceForMachine(application_id, cfx_prefs_);
  }

  Boolean IsManagedPolicyAvailable(CFStringRef key) {
    if (!machine_scope_) {
      return YES;
    }

    return
        [machine_scope_ copyValueForKey:base::apple::CFToNSPtrCast(key)] != nil;
  }

 private:
  _CFXPreferences* __strong cfx_prefs_;
  CFPrefsManagedSource* __strong machine_scope_;
};

MacPreferences::MacPreferences()
    : policy_scope_(std::make_unique<PolicyScope>()) {}
MacPreferences::~MacPreferences() = default;

Boolean MacPreferences::AppSynchronize(CFStringRef application_id) {
  policy_scope_->Init(application_id);
  return CFPreferencesAppSynchronize(application_id);
}

CFPropertyListRef MacPreferences::CopyAppValue(CFStringRef key,
                                               CFStringRef application_id) {
  return CFPreferencesCopyAppValue(key, application_id);
}

Boolean MacPreferences::AppValueIsForced(CFStringRef key,
                                         CFStringRef application_id) {
  return CFPreferencesAppValueIsForced(key, application_id);
}

Boolean MacPreferences::IsManagedPolicyAvailableForMachineScope(
    CFStringRef key) {
  return policy_scope_->IsManagedPolicyAvailable(key);
}
