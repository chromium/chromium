# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=line-too-long
# Disable this warning because shortening the lines in this file to 80
# characters will negatively impact readability as the strings will no longer
# look the same as the output files.

EXPECTED_CLOUD_POLICY_PROTOBUF = '''
syntax = "proto2";

%(full_runtime_comment)soption optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

import "policy_common_definitions%(full_runtime_suffix)s.proto";
message CloudPolicySettings {
  optional StringPolicyProto ExampleStringPolicy = 3;
  optional BooleanPolicyProto ExampleBoolPolicy = 4;
  optional BooleanPolicyProto ExampleBoolMergeMetapolicy = 5;
  optional BooleanPolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional BooleanPolicyProto CloudOnlyPolicy = 7;
}
'''

EXPECTED_CHROME_SETTINGS_PROTOBUF = """
syntax = "proto2";

%(full_runtime_comment)soption optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

// For StringList and PolicyOptions.
import "policy_common_definitions%(full_runtime_suffix)s.proto";

// PBs for individual settings.

// ExampleStringPolicy caption
//
// ExampleStringPolicy desc
//
// Supported on: chrome_os
message ExampleStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ExampleStringPolicy = 2;
}

// ExampleBoolPolicy caption
//
// ExampleBoolPolicy desc
//
// Supported on: chrome_os
message ExampleBoolPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPolicy = 2;
}

// ExampleBoolMergeMetapolicy caption
//
// ExampleBoolMergeMetapolicy desc
//
// Supported on: chrome_os
message ExampleBoolMergeMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolMergeMetapolicy = 2;
}

// ExampleBoolPrecedenceMetapolicy caption
//
// ExampleBoolPrecedenceMetapolicy desc
//
// Supported on: chrome_os
message ExampleBoolPrecedenceMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPrecedenceMetapolicy = 2;
}

// CloudOnlyPolicy caption
//
// CloudOnlyPolicy desc
//
// Supported on: android, chrome_os
message CloudOnlyPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool CloudOnlyPolicy = 2;
}

// --------------------------------------------------
// Big wrapper PB containing the above groups.

message ChromeSettingsProto {
  optional ExampleStringPolicyProto ExampleStringPolicy = 3;
  optional ExampleBoolPolicyProto ExampleBoolPolicy = 4;
  optional ExampleBoolMergeMetapolicyProto ExampleBoolMergeMetapolicy = 5;
  optional ExampleBoolPrecedenceMetapolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional CloudOnlyPolicyProto CloudOnlyPolicy = 7;
}
"""

EXPECTED_POLICY_PROTO = '''\
// ExampleStringPolicy caption
//
// ExampleStringPolicy desc
//
// Supported on: chrome_os
message ExampleStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ExampleStringPolicy = 2;
}
'''

EXPECTED_POLICY_CONSTANTS_HEADER = '''
#ifndef COMPONENTS_POLICY_POLICY_CONSTANTS_H_
#define COMPONENTS_POLICY_POLICY_CONSTANTS_H_

#include <cstdint>
#include <string>

#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace policy {

namespace internal {
struct SchemaData;
}
%(windows_only_part)s
#if defined(OS_CHROMEOS)
// Sets default profile policies values for enterprise users.
void SetEnterpriseUsersProfileDefaults(PolicyMap* policy_map);
// Sets default system-wide policies values for enterprise users.
void SetEnterpriseUsersSystemWideDefaults(PolicyMap* policy_map);
// Sets all default values for enterprise users.
void SetEnterpriseUsersDefaults(PolicyMap* policy_map);
#endif

// Returns the PolicyDetails for |policy| if |policy| is a known
// Chrome policy, otherwise returns nullptr.
const PolicyDetails* GetChromePolicyDetails(
const std::string& policy);

// Returns the schema data of the Chrome policy schema.
const internal::SchemaData* GetChromeSchemaData();

// Key names for the policy settings.
namespace key {

extern const char kExampleStringPolicy[];
extern const char kExampleBoolPolicy[];
extern const char kExampleBoolMergeMetapolicy[];
extern const char kExampleBoolPrecedenceMetapolicy[];
extern const char kCloudOnlyPolicy[];

}  // namespace key

// Group names for the policy settings.
namespace group {


}  // namespace group

struct AtomicGroup {
  const short id;
  const char* policy_group;
  const char* const* policies;
};

extern const AtomicGroup kPolicyAtomicGroupMappings[];

extern const size_t kPolicyAtomicGroupMappingsLength;

// Arrays of metapolicies.
namespace metapolicy {

extern const char* const kMerge[1];
extern const char* const kPrecedence[1];

}  // namespace metapolicy

enum class StringPolicyType {
  STRING,
  JSON,
  EXTERNAL,
};

// Read access to the protobufs of all supported boolean user policies.
struct BooleanPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::BooleanPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
};
extern const BooleanPolicyAccess kBooleanPolicyAccess[];

// Read access to the protobufs of all supported integer user policies.
struct IntegerPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::IntegerPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
};
extern const IntegerPolicyAccess kIntegerPolicyAccess[];

// Read access to the protobufs of all supported string user policies.
struct StringPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::StringPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
  const StringPolicyType type;
};
extern const StringPolicyAccess kStringPolicyAccess[];

// Read access to the protobufs of all supported stringlist user policies.
struct StringListPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::StringListPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
};
extern const StringListPolicyAccess kStringListPolicyAccess[];

constexpr int64_t kDevicePolicyExternalDataResourceCacheSize = 0;

}  // namespace policy

#endif  // COMPONENTS_POLICY_POLICY_CONSTANTS_H_
'''

POLICY_CONSTANTS_HEADER_WIN_ONLY_PART = '''
// The windows registry path where Chrome policy configuration resides.
extern const wchar_t kRegistryChromePolicyKey[];'''

EXPECTED_POLICY_CONSTANTS_SOURCE = '''\
#include "components/policy/policy_constants.h"

#include <algorithm>
#include <climits>
#include <memory>

#include "base/check_op.h"
#include "base/stl_util.h"  // base::size()
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_internal.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/risk_tag.h"

namespace em = enterprise_management;

namespace policy {

const __attribute__((unused)) PolicyDetails kChromePolicyDetails[] = {
// is_deprecated is_future is_device_policy id max_external_data_size, risk tags
  // ExampleStringPolicy
  { false,        false,    false,              1,                     0, {  } },
  // ExampleBoolPolicy
  { false,        false,    false,              2,                     0, {  } },
  // ExampleBoolMergeMetapolicy
  { false,        false,    false,              3,                     0, {  } },
  // ExampleBoolPrecedenceMetapolicy
  { false,        false,    false,              4,                     0, {  } },
  // CloudOnlyPolicy
  { false,        false,    false,              5,                     0, {  } },
};

const internal::SchemaNode kSchemas[] = {
//  Type                           Extra  IsSensitiveValue HasSensitiveChildren
  { base::Value::Type::DICTIONARY,     0, false,           false },  // root node
  { base::Value::Type::BOOLEAN,       -1, false,           false },  // simple type: boolean
  { base::Value::Type::STRING,        -1, false,           false },  // simple type: string
};

const internal::PropertyNode kPropertyNodes[] = {
//  Property                                                             Schema
  { key::kCloudOnlyPolicy,                                                1 },
  { key::kExampleBoolMergeMetapolicy,                                     1 },
  { key::kExampleBoolPolicy,                                              1 },
  { key::kExampleBoolPrecedenceMetapolicy,                                1 },
  { key::kExampleStringPolicy,                                            2 },
};

const internal::PropertiesNode kProperties[] = {
//  Begin    End  PatternEnd  RequiredBegin  RequiredEnd  Additional Properties
  {     0,     5,     5,     0,          0,    -1 },  // root node
};

const internal::SchemaData* GetChromeSchemaData() {
  static const internal::SchemaData kChromeSchemaData = {
    kSchemas,
    kPropertyNodes,
    kProperties,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
    -1,  // validation_schema root index
  };

  return &kChromeSchemaData;
}


namespace {
bool CompareKeys(const internal::PropertyNode& node,
                 const std::string& key) {
  return node.key < key;
}

}  // namespace
%(windows_only_part)s
#if defined(OS_CHROMEOS)
void SetEnterpriseUsersProfileDefaults(PolicyMap* policy_map) {

}

void SetEnterpriseUsersSystemWideDefaults(PolicyMap* policy_map) {

}

void SetEnterpriseUsersDefaults(PolicyMap* policy_map) {
  SetEnterpriseUsersProfileDefaults(policy_map);
  SetEnterpriseUsersSystemWideDefaults(policy_map);
}
#endif

const PolicyDetails* GetChromePolicyDetails(const std::string& policy) {
  // First index in kPropertyNodes of the Chrome policies.
  static const int begin_index = 0;
  // One-past-the-end of the Chrome policies in kPropertyNodes.
  static const int end_index = 5;
  const internal::PropertyNode* begin =
     kPropertyNodes + begin_index;
  const internal::PropertyNode* end = kPropertyNodes + end_index;
  const internal::PropertyNode* it =
      std::lower_bound(begin, end, policy, CompareKeys);
  if (it == end || it->key != policy)
    return nullptr;
  // This relies on kPropertyNodes from begin_index to end_index
  // having exactly the same policies (and in the same order) as
  // kChromePolicyDetails, so that binary searching on the first
  // gets the same results as a binary search on the second would.
  // However, kPropertyNodes has the policy names and
  // kChromePolicyDetails doesn't, so we obtain the index into
  // the second array by searching the first to avoid duplicating
  // the policy name pointers.
  // Offsetting |it| from |begin| here obtains the index we're
  // looking for.
  size_t index = it - begin;
  CHECK_LT(index, base::size(kChromePolicyDetails));
  return kChromePolicyDetails + index;
}

namespace key {

const char kExampleStringPolicy[] = "ExampleStringPolicy";
const char kExampleBoolPolicy[] = "ExampleBoolPolicy";
const char kExampleBoolMergeMetapolicy[] = "ExampleBoolMergeMetapolicy";
const char kExampleBoolPrecedenceMetapolicy[] = "ExampleBoolPrecedenceMetapolicy";
const char kCloudOnlyPolicy[] = "CloudOnlyPolicy";

}  // namespace key

namespace group {


namespace {


}  // namespace

}  // namespace group

const AtomicGroup kPolicyAtomicGroupMappings[] = {
};

const size_t kPolicyAtomicGroupMappingsLength = 0;

namespace metapolicy {

const char* const kMerge[1] = {
  key::kExampleBoolMergeMetapolicy,
};

const char* const kPrecedence[1] = {
  key::kExampleBoolPrecedenceMetapolicy,
};

}  // namespace metapolicy

const BooleanPolicyAccess kBooleanPolicyAccess[] = {
  {key::kExampleBoolPolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolpolicy,
   &em::CloudPolicySettings::exampleboolpolicy},
  {key::kExampleBoolMergeMetapolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolmergemetapolicy,
   &em::CloudPolicySettings::exampleboolmergemetapolicy},
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolprecedencemetapolicy,
   &em::CloudPolicySettings::exampleboolprecedencemetapolicy},
  {key::kCloudOnlyPolicy,
   false,
   &em::CloudPolicySettings::has_cloudonlypolicy,
   &em::CloudPolicySettings::cloudonlypolicy},
  {nullptr, false, nullptr, nullptr},
};

const IntegerPolicyAccess kIntegerPolicyAccess[] = {
  {nullptr, false, nullptr, nullptr},
};

const StringPolicyAccess kStringPolicyAccess[] = {
  {key::kExampleStringPolicy,
   false,
   &em::CloudPolicySettings::has_examplestringpolicy,
   &em::CloudPolicySettings::examplestringpolicy,
   StringPolicyType::STRING},
  {nullptr, false, nullptr, nullptr},
};

const StringListPolicyAccess kStringListPolicyAccess[] = {
  {nullptr, false, nullptr, nullptr},
};


}  // namespace policy
'''

POLICY_CONSTANTS_SOURCE_WIN_ONLY_PART = '''
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Google\\\\Chrome";
#else
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Chromium";
#endif
'''

EXPECTED_CROS_POLICY_CONSTANTS_HEADER = '''
#ifndef __BINDINGS_POLICY_CONSTANTS_H_
#define __BINDINGS_POLICY_CONSTANTS_H_

namespace enterprise_management {
class CloudPolicySettings;
class BooleanPolicyProto;
class IntegerPolicyProto;
class StringPolicyProto;
class StringListPolicyProto;
}  // namespace enterprise_management

namespace policy {

// Registry key names for user and device policies.
namespace key {

extern const char kExampleStringPolicy[];
extern const char kExampleBoolPolicy[];
extern const char kExampleBoolMergeMetapolicy[];
extern const char kExampleBoolPrecedenceMetapolicy[];
extern const char kCloudOnlyPolicy[];

}  // namespace key

// NULL-terminated list of device policy registry key names.
extern const char* kDevicePolicyKeys[];

// Access to the mutable protobuf function of all supported boolean user
// policies.
struct BooleanPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::BooleanPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
extern const BooleanPolicyAccess kBooleanPolicyAccess[];

// Access to the mutable protobuf function of all supported integer user
// policies.
struct IntegerPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::IntegerPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
extern const IntegerPolicyAccess kIntegerPolicyAccess[];

// Access to the mutable protobuf function of all supported string user
// policies.
struct StringPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
extern const StringPolicyAccess kStringPolicyAccess[];

// Access to the mutable protobuf function of all supported stringlist user
// policies.
struct StringListPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringListPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
extern const StringListPolicyAccess kStringListPolicyAccess[];

}  // namespace policy

#endif  // __BINDINGS_POLICY_CONSTANTS_H_
'''

EXPECTED_CROS_POLICY_CONSTANTS_SOURCE = '''
#include "bindings/cloud_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace key {

const char kExampleStringPolicy[] = "ExampleStringPolicy";
const char kExampleBoolPolicy[] = "ExampleBoolPolicy";
const char kExampleBoolMergeMetapolicy[] = "ExampleBoolMergeMetapolicy";
const char kExampleBoolPrecedenceMetapolicy[] = "ExampleBoolPrecedenceMetapolicy";
const char kCloudOnlyPolicy[] = "CloudOnlyPolicy";

}  // namespace key

const char* kDevicePolicyKeys[] = {

  nullptr};

constexpr BooleanPolicyAccess kBooleanPolicyAccess[] = {
  {key::kExampleBoolPolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolpolicy},
  {key::kExampleBoolMergeMetapolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolmergemetapolicy},
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolprecedencemetapolicy},
  {key::kCloudOnlyPolicy,
   false,
   &em::CloudPolicySettings::mutable_cloudonlypolicy},
  {nullptr, false, nullptr},
};

constexpr IntegerPolicyAccess kIntegerPolicyAccess[] = {
  {nullptr, false, nullptr},
};

constexpr StringPolicyAccess kStringPolicyAccess[] = {
  {key::kExampleStringPolicy,
   false,
   &em::CloudPolicySettings::mutable_examplestringpolicy},
  {nullptr, false, nullptr},
};

constexpr StringListPolicyAccess kStringListPolicyAccess[] = {
  {nullptr, false, nullptr},
};

}  // namespace policy
'''

EXPECTED_APP_RESTRICTIONS_XML = '''
<restrictions xmlns:android="http://schemas.android.com/apk/res/android">

    <restriction
        android:key="ExampleStringPolicy"
        android:title="@string/ExampleStringPolicyTitle"
        android:description="@string/ExampleStringPolicyDesc"
        android:restrictionType="string"/>

    <restriction
        android:key="ExampleBoolPolicy"
        android:title="@string/ExampleBoolPolicyTitle"
        android:description="@string/ExampleBoolPolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleBoolMergeMetapolicy"
        android:title="@string/ExampleBoolMergeMetapolicyTitle"
        android:description="@string/ExampleBoolMergeMetapolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleBoolPrecedenceMetapolicy"
        android:title="@string/ExampleBoolPrecedenceMetapolicyTitle"
        android:description="@string/ExampleBoolPrecedenceMetapolicyDesc"
        android:restrictionType="bool"/>

</restrictions>'''
