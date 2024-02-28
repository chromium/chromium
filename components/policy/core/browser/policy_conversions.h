// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/webui/web_ui_util.h"

namespace policy {

class PolicyConversionsClient;

extern const POLICY_EXPORT webui::LocalizedString
    kPolicySources[POLICY_SOURCE_COUNT];

extern const POLICY_EXPORT char kIdKey[];
extern const POLICY_EXPORT char kNameKey[];
extern const POLICY_EXPORT char kPoliciesKey[];
extern const POLICY_EXPORT char kPolicyNamesKey[];
extern const POLICY_EXPORT char kChromePoliciesId[];
extern const POLICY_EXPORT char kChromePoliciesName[];

#if !BUILDFLAG(IS_CHROMEOS)
extern const POLICY_EXPORT char kPrecedencePoliciesId[];
extern const POLICY_EXPORT char kPrecedencePoliciesName[];
#endif  // !BUILDFLAG(IS_CHROMEOS)

// A convenience class to retrieve all policies values.
// Usage example:
// PolicyConversions
//    .EnableConvertTypes(false)     // Using enable* functions to turn on/off
//    .EnableConvertValues(false)    // some features. All enable* functions are
//    .EnableConvertValues(true)     // optional.
//    .UseChromePolicyConversions()  // Choose different delegate if needed.
//    .ToValueDict();                // Choose the data format in the end.
class POLICY_EXPORT PolicyConversions {
 public:
  // Maps known policy names to their schema. If a policy is not present, it is
  // not known (either through policy_templates.json or through an extension's
  // managed storage schema).
  using PolicyToSchemaMap = base::flat_map<std::string, Schema>;

  // Delegate class that controls the structure and content of the output.
  class Delegate {
   public:
    explicit Delegate(PolicyConversionsClient* client);
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate();

    virtual base::Value::Dict ToValueDict() = 0;

   protected:
    PolicyConversionsClient* client() { return client_; }

   private:
    raw_ptr<PolicyConversionsClient> client_;
  };

  // |client| provides embedder-specific policy information and must not be
  // nullptr.
  explicit PolicyConversions(std::unique_ptr<PolicyConversionsClient> client);
  PolicyConversions(const PolicyConversions&) = delete;
  PolicyConversions& operator=(const PolicyConversions&) = delete;
  virtual ~PolicyConversions();

  // Set to get policy types as human friendly string instead of enum integer.
  // Policy types includes policy source, policy scope and policy level.
  // Enabled by default.
  PolicyConversions& EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  PolicyConversions& EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  PolicyConversions& EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  PolicyConversions& EnableUserPolicies(bool enabled);
  // Set to drop the policies of which value is a default one set by the policy
  // provider. Disabled by default.
  PolicyConversions& SetDropDefaultValues(bool enabled);
  // Set to show policy values set by machine scope sources including CBCM or
  // GPO. When set to false, policies are still included, but values and errors
  // will be hidden. Used when caller don't have permission to view those
  // values. Enabled by default.
  PolicyConversions& EnableShowMachineValues(bool enabled);

  // Switch to Chrome policy conversion to get Chrome policies.
  // Chrome policy conversion can't be used to return device local account
  // policies and device info.
  PolicyConversions& UseChromePolicyConversions();

  // Returns the policy data as a JSON string;
  std::string ToJSON();

  base::Value::Dict ToValueDict();

 private:
  std::unique_ptr<PolicyConversionsClient> client_;
  std::unique_ptr<Delegate> delegate_;
};

// Used to export all policies.
class POLICY_EXPORT DefaultPolicyConversions
    : public PolicyConversions::Delegate {
 public:
  explicit DefaultPolicyConversions(PolicyConversionsClient* client);
  DefaultPolicyConversions(const DefaultPolicyConversions&) = delete;
  DefaultPolicyConversions& operator=(const DefaultPolicyConversions&) = delete;
  ~DefaultPolicyConversions() override;

  base::Value::Dict ToValueDict() override;

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  base::Value::Dict GetExtensionPolicies();
  base::Value::Dict GetExtensionPolicies(PolicyDomain policy_domain);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Value::Dict GetDeviceLocalAccountPolicies();
#endif
};

// Used to export all Chrome policies. It also splits precedence
// policies into a different section.
class POLICY_EXPORT ChromePolicyConversions
    : public PolicyConversions::Delegate {
 public:
  explicit ChromePolicyConversions(PolicyConversionsClient* client);
  ChromePolicyConversions(const ChromePolicyConversions&) = delete;
  ChromePolicyConversions& operator=(const ChromePolicyConversions&) = delete;
  ~ChromePolicyConversions() override;

  base::Value::Dict ToValueDict() override;

 private:
  base::Value::Dict GetChromePolicies();
#if !BUILDFLAG(IS_CHROMEOS)
  base::Value::Dict GetPrecedencePolicies();
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_CONVERSIONS_H_
