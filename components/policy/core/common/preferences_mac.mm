// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/preferences_mac.h"
#include "components/policy/policy_constants.h"

// `CFPrefsManagedSource` and `_CFXPreferences` are used to determine the scope
// of a policy. A policy can be read with `copyValueForKey()` below with
// `kCFPreferencesAnyUser` will be treated as machine scope policy. Otherwise,
// it will be user scope policy. The implementation of these two interfaces are
// only available during runtime and will be obtained with `objc_getClass()`.
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

base::scoped_nsobject<_CFXPreferences> CreateCFXPrefs() {
  // _CFXPreferences is only available during runtime.
  return base::scoped_nsobject<_CFXPreferences>(
      [[objc_getClass("_CFXPreferences") alloc] init]);
}

base::scoped_nsobject<CFPrefsManagedSource>
CreateCFPrefsManagedSourceForMachine(CFStringRef application_id, id cfxPrefs) {
  if (!cfxPrefs)
    return base::scoped_nsobject<CFPrefsManagedSource>();

  // CFPrefsManagedSource is only available during runtime.
  base::scoped_nsobject<CFPrefsManagedSource> source(
      [objc_getClass("CFPrefsManagedSource") alloc]);

  if (![source respondsToSelector:@selector
               (initWithDomain:
                          user:byHost:containerPath:containingPreferences:)] ||
      ![source respondsToSelector:@selector(copyValueForKey:)]) {
    return base::scoped_nsobject<CFPrefsManagedSource>();
  }

  [source initWithDomain:base::mac::CFToNSCast(application_id)
                       user:base::mac::CFToNSCast(kCFPreferencesAnyUser)
                     byHost:YES
              containerPath:nil
      containingPreferences:cfxPrefs];
  return source;
}

class MachinePolicyScope : public MacPreferences::PolicyScope {
 public:
  MachinePolicyScope() {
    // Policy scope detection is enabled by default.
    Enable(true);
  }
  ~MachinePolicyScope() override = default;

  void Init(CFStringRef application_id) override {
    if (!enable_)
      return;
    if (!cfx_prefs_)
      cfx_prefs_.reset(CreateCFXPrefs());
    machine_scope_.reset(
        CreateCFPrefsManagedSourceForMachine(application_id, cfx_prefs_));
  }

  Boolean IsManagedPolicyAvailable(CFStringRef key) override {
    if (!enable_)
      return YES;
    if (!machine_scope_)
      return YES;
    return base::scoped_nsobject<id>([machine_scope_
               copyValueForKey:base::mac::CFToNSCast(key)]) != nil;
  }

  void Enable(bool enable) override { enable_ = enable; }

 private:
  base::scoped_nsobject<_CFXPreferences> cfx_prefs_;
  base::scoped_nsobject<CFPrefsManagedSource> machine_scope_;
  bool enable_ = true;
};

}  // namespace

MacPreferences::MacPreferences()
    : policy_scope_(std::make_unique<MachinePolicyScope>()) {}
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

void MacPreferences::LoadPolicyScopeDetectionPolicy(
    CFStringRef application_id) {
  base::ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(policy::key::kPolicyScopeDetection));
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      CopyAppValue(name, application_id));
  if (!value)
    return;
  if (!AppValueIsForced(name, application_id))
    return;

  // The policy scope detection is force turned off by policy.
  if (!policy::PropertyToValue(value)->GetIfBool().value_or(true))
    policy_scope_->Enable(false);
}
