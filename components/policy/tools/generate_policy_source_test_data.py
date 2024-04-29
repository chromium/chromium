# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=line-too-long
# Disable this warning because shortening the lines in this file to 80
# characters will negatively impact readability as the strings will no longer
# look the same as the output files.

EXPECTED_CLOUD_POLICY_PROTOBUF = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

import "policy_common_definitions.proto";

message CloudPolicySubProto1 {
  optional BooleanPolicyProto ChunkOneFirstFieldBooleanPolicy = 1;
  optional BooleanPolicyProto ChunkOneLastFieldBooleanPolicy = 800;
}

message CloudPolicySubProto2 {
  optional StringPolicyProto ChunkTwoFirstFieldStringPolicy = 1;
  optional StringPolicyProto ChunkTwoLastFieldStringPolicy = 800;
}

message CloudPolicySettings {
  optional StringPolicyProto ExampleStringPolicy = 3;
  optional BooleanPolicyProto ExampleBoolPolicy = 4;
  optional BooleanPolicyProto ExampleBoolMergeMetapolicy = 5;
  optional BooleanPolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional BooleanPolicyProto CloudOnlyPolicy = 7;
  optional StringPolicyProto CloudManagementEnrollmentToken = 8;
  optional BooleanPolicyProto ChunkZeroLastFieldBooleanPolicy = 1042;
  optional CloudPolicySubProto1 subProto1 = 1043;
  optional CloudPolicySubProto2 subProto2 = 1044;
}
'''

EXPECTED_CLOUD_POLICY_PROTOBUF_NO_CHUNKING = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

import "policy_common_definitions.proto";

message CloudPolicySettings {
  optional StringPolicyProto ExampleStringPolicy = 3;
  optional BooleanPolicyProto ExampleBoolPolicy = 4;
  optional BooleanPolicyProto ExampleBoolMergeMetapolicy = 5;
  optional BooleanPolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional BooleanPolicyProto CloudOnlyPolicy = 7;
  optional StringPolicyProto CloudManagementEnrollmentToken = 8;
  optional BooleanPolicyProto ChunkZeroLastFieldBooleanPolicy = 1042;
  optional BooleanPolicyProto ChunkOneFirstFieldBooleanPolicy = 1043;
  optional BooleanPolicyProto ChunkOneLastFieldBooleanPolicy = 1842;
  optional StringPolicyProto ChunkTwoFirstFieldStringPolicy = 1843;
  optional StringPolicyProto ChunkTwoLastFieldStringPolicy = 2642;
}
'''

EXPECTED_CHROME_SETTINGS_PROTOBUF = """
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

// For StringList and PolicyOptions.
import "policy_common_definitions.proto";

// PBs for individual settings.

// ExampleStringPolicy caption
//
// ExampleStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ExampleStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ExampleStringPolicy = 2;
}

// ExampleBoolPolicy caption
//
// ExampleBoolPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ExampleBoolPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPolicy = 2;
}

// ExampleBoolMergeMetapolicy caption
//
// ExampleBoolMergeMetapolicy desc
//
// Supported on: android, chrome_os, fuchsia, ios, linux, mac, win
message ExampleBoolMergeMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolMergeMetapolicy = 2;
}

// ExampleBoolPrecedenceMetapolicy caption
//
// ExampleBoolPrecedenceMetapolicy desc
//
// Supported on: android, chrome_os, ios, linux, mac, win
message ExampleBoolPrecedenceMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPrecedenceMetapolicy = 2;
}

// CloudOnlyPolicy caption
//
// CloudOnlyPolicy desc
//
// Supported on: android, chrome_os, linux, mac, win
message CloudOnlyPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool CloudOnlyPolicy = 2;
}

// CloudManagementEnrollmentToken caption
//
// CloudManagementEnrollmentToken desc
//
// Supported on: android, chrome_os, linux, mac, win
message CloudManagementEnrollmentTokenProto {
  optional PolicyOptions policy_options = 1;
  optional string CloudManagementEnrollmentToken = 2;
}

// DeprecatedNotGenerated caption
//
// DeprecatedNotGenerated desc
//
// Supported on:
message DeprecatedNotGeneratedProto {
  optional PolicyOptions policy_options = 1;
  optional string DeprecatedNotGenerated = 2;
}

// UnsupportedPolicy caption
//
// UnsupportedPolicy desc
//
// Supported on:
message UnsupportedPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string UnsupportedPolicy = 2;
}

// ChunkZeroLastFieldBooleanPolicy caption
//
// ChunkZeroLastFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkZeroLastFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkZeroLastFieldBooleanPolicy = 2;
}

// ChunkOneFirstFieldBooleanPolicy caption
//
// ChunkOneFirstFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkOneFirstFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkOneFirstFieldBooleanPolicy = 2;
}

// ChunkOneLastFieldBooleanPolicy caption
//
// ChunkOneLastFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkOneLastFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkOneLastFieldBooleanPolicy = 2;
}

// ChunkTwoFirstFieldStringPolicy caption
//
// ChunkTwoFirstFieldStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ChunkTwoFirstFieldStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ChunkTwoFirstFieldStringPolicy = 2;
}

// ChunkTwoLastFieldStringPolicy caption
//
// ChunkTwoLastFieldStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ChunkTwoLastFieldStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ChunkTwoLastFieldStringPolicy = 2;
}

// --------------------------------------------------
// PBs for policies with ID > 1040.

message ChromeSettingsSubProto1 {
  optional ChunkOneFirstFieldBooleanPolicyProto ChunkOneFirstFieldBooleanPolicy = 1;
  optional ChunkOneLastFieldBooleanPolicyProto ChunkOneLastFieldBooleanPolicy = 800;
}

message ChromeSettingsSubProto2 {
  optional ChunkTwoFirstFieldStringPolicyProto ChunkTwoFirstFieldStringPolicy = 1;
  optional ChunkTwoLastFieldStringPolicyProto ChunkTwoLastFieldStringPolicy = 800;
}

// --------------------------------------------------
// Big wrapper PB containing the above groups.

message ChromeSettingsProto {
  optional ExampleStringPolicyProto ExampleStringPolicy = 3;
  optional ExampleBoolPolicyProto ExampleBoolPolicy = 4;
  optional ExampleBoolMergeMetapolicyProto ExampleBoolMergeMetapolicy = 5;
  optional ExampleBoolPrecedenceMetapolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional CloudOnlyPolicyProto CloudOnlyPolicy = 7;
  optional CloudManagementEnrollmentTokenProto CloudManagementEnrollmentToken = 8;
  optional DeprecatedNotGeneratedProto DeprecatedNotGenerated = 10;
  optional UnsupportedPolicyProto UnsupportedPolicy = 11;
  optional ChunkZeroLastFieldBooleanPolicyProto ChunkZeroLastFieldBooleanPolicy = 1042;
  optional ChromeSettingsSubProto1 subProto1 = 1043;
  optional ChromeSettingsSubProto2 subProto2 = 1044;
}
"""

EXPECTED_CHROME_SETTINGS_PROTOBUF_NO_CHUNKING = """
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

option go_package="chromium/policy/enterprise_management_proto";

// For StringList and PolicyOptions.
import "policy_common_definitions.proto";

// PBs for individual settings.

// ExampleStringPolicy caption
//
// ExampleStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ExampleStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ExampleStringPolicy = 2;
}

// ExampleBoolPolicy caption
//
// ExampleBoolPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ExampleBoolPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPolicy = 2;
}

// ExampleBoolMergeMetapolicy caption
//
// ExampleBoolMergeMetapolicy desc
//
// Supported on: android, chrome_os, fuchsia, ios, linux, mac, win
message ExampleBoolMergeMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolMergeMetapolicy = 2;
}

// ExampleBoolPrecedenceMetapolicy caption
//
// ExampleBoolPrecedenceMetapolicy desc
//
// Supported on: android, chrome_os, ios, linux, mac, win
message ExampleBoolPrecedenceMetapolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ExampleBoolPrecedenceMetapolicy = 2;
}

// CloudOnlyPolicy caption
//
// CloudOnlyPolicy desc
//
// Supported on: android, chrome_os, linux, mac, win
message CloudOnlyPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool CloudOnlyPolicy = 2;
}

// CloudManagementEnrollmentToken caption
//
// CloudManagementEnrollmentToken desc
//
// Supported on: android, chrome_os, linux, mac, win
message CloudManagementEnrollmentTokenProto {
  optional PolicyOptions policy_options = 1;
  optional string CloudManagementEnrollmentToken = 2;
}

// DeprecatedNotGenerated caption
//
// DeprecatedNotGenerated desc
//
// Supported on:
message DeprecatedNotGeneratedProto {
  optional PolicyOptions policy_options = 1;
  optional string DeprecatedNotGenerated = 2;
}

// UnsupportedPolicy caption
//
// UnsupportedPolicy desc
//
// Supported on:
message UnsupportedPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string UnsupportedPolicy = 2;
}

// ChunkZeroLastFieldBooleanPolicy caption
//
// ChunkZeroLastFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkZeroLastFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkZeroLastFieldBooleanPolicy = 2;
}

// ChunkOneFirstFieldBooleanPolicy caption
//
// ChunkOneFirstFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkOneFirstFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkOneFirstFieldBooleanPolicy = 2;
}

// ChunkOneLastFieldBooleanPolicy caption
//
// ChunkOneLastFieldBooleanPolicy desc.
//
// Supported on: chrome_os, linux, mac, win
message ChunkOneLastFieldBooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool ChunkOneLastFieldBooleanPolicy = 2;
}

// ChunkTwoFirstFieldStringPolicy caption
//
// ChunkTwoFirstFieldStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ChunkTwoFirstFieldStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ChunkTwoFirstFieldStringPolicy = 2;
}

// ChunkTwoLastFieldStringPolicy caption
//
// ChunkTwoLastFieldStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
message ChunkTwoLastFieldStringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string ChunkTwoLastFieldStringPolicy = 2;
}

// --------------------------------------------------
// Big wrapper PB containing the above groups.

message ChromeSettingsProto {
  optional ExampleStringPolicyProto ExampleStringPolicy = 3;
  optional ExampleBoolPolicyProto ExampleBoolPolicy = 4;
  optional ExampleBoolMergeMetapolicyProto ExampleBoolMergeMetapolicy = 5;
  optional ExampleBoolPrecedenceMetapolicyProto ExampleBoolPrecedenceMetapolicy = 6;
  optional CloudOnlyPolicyProto CloudOnlyPolicy = 7;
  optional CloudManagementEnrollmentTokenProto CloudManagementEnrollmentToken = 8;
  optional DeprecatedNotGeneratedProto DeprecatedNotGenerated = 10;
  optional UnsupportedPolicyProto UnsupportedPolicy = 11;
  optional ChunkZeroLastFieldBooleanPolicyProto ChunkZeroLastFieldBooleanPolicy = 1042;
  optional ChunkOneFirstFieldBooleanPolicyProto ChunkOneFirstFieldBooleanPolicy = 1043;
  optional ChunkOneLastFieldBooleanPolicyProto ChunkOneLastFieldBooleanPolicy = 1842;
  optional ChunkTwoFirstFieldStringPolicyProto ChunkTwoFirstFieldStringPolicy = 1843;
  optional ChunkTwoLastFieldStringPolicyProto ChunkTwoLastFieldStringPolicy = 2642;
}
"""

EXPECTED_POLICY_PROTO = '''\
// ExampleStringPolicy caption
//
// ExampleStringPolicy desc
//
// Supported on: chrome_os, linux, mac, win
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

namespace enterprise_management {
class BooleanPolicyProto;
class CloudPolicySettings;
class IntegerPolicyProto;
class StringListPolicyProto;
class StringPolicyProto;
}

namespace em = enterprise_management;

namespace policy {

namespace internal {
struct SchemaData;
}
%(windows_only_part)s
#if BUILDFLAG(IS_CHROMEOS)
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
extern const char kCloudManagementEnrollmentToken[];
extern const char kChunkZeroLastFieldBooleanPolicy[];
extern const char kChunkOneFirstFieldBooleanPolicy[];
extern const char kChunkOneLastFieldBooleanPolicy[];
extern const char kChunkTwoFirstFieldStringPolicy[];
extern const char kChunkTwoLastFieldStringPolicy[];

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
  bool (*has_proto)(const em::CloudPolicySettings& policy);
  const em::BooleanPolicyProto& (*get_proto)(
      const em::CloudPolicySettings& policy);
};
extern const std::array<BooleanPolicyAccess, 7> kBooleanPolicyAccess;

// Read access to the protobufs of all supported integer user policies.
struct IntegerPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (*has_proto)(const em::CloudPolicySettings& policy);
  const em::IntegerPolicyProto& (*get_proto)(
      const em::CloudPolicySettings& policy);
};
extern const std::array<IntegerPolicyAccess, 0> kIntegerPolicyAccess;

// Read access to the protobufs of all supported string user policies.
struct StringPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (*has_proto)(const em::CloudPolicySettings& policy);
  const em::StringPolicyProto& (*get_proto)(
      const em::CloudPolicySettings& policy);
  const StringPolicyType type;
};
extern const std::array<StringPolicyAccess, 4> kStringPolicyAccess;

// Read access to the protobufs of all supported stringlist user policies.
struct StringListPolicyAccess {
  const char* policy_key;
  bool per_profile;
  bool (*has_proto)(const em::CloudPolicySettings& policy);
  const em::StringListPolicyProto& (*get_proto)(
      const em::CloudPolicySettings& policy);
};
extern const std::array<StringListPolicyAccess, 0> kStringListPolicyAccess;

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
#include <iterator>
#include <memory>

#include "base/check_op.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_internal.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/risk_tag.h"

namespace policy {

[[maybe_unused]] const PolicyDetails kChromePolicyDetails[] = {
// is_deprecated is_future scope id max_external_data_size, risk tags
  // ExampleStringPolicy
  { false,        false,    kBrowser,           1,                     0, {  } },
  // ExampleBoolPolicy
  { false,        false,    kBrowser,           2,                     0, {  } },
  // ExampleBoolMergeMetapolicy
  { false,        false,    kBrowser,           3,                     0, {  } },
  // ExampleBoolPrecedenceMetapolicy
  { false,        false,    kBrowser,           4,                     0, {  } },
  // CloudOnlyPolicy
  { false,        false,    kBrowser,           5,                     0, {  } },
  // CloudManagementEnrollmentToken
  { false,        false,    kBrowser,           6,                     0, {  } },
  // ChunkZeroLastFieldBooleanPolicy
  { false,        false,    kBrowser,        1040,                     0, {  } },
  // ChunkOneFirstFieldBooleanPolicy
  { false,        false,    kBrowser,        1041,                     0, {  } },
  // ChunkOneLastFieldBooleanPolicy
  { false,        false,    kBrowser,        1840,                     0, {  } },
  // ChunkTwoFirstFieldStringPolicy
  { false,        false,    kBrowser,        1841,                     0, {  } },
  // ChunkTwoLastFieldStringPolicy
  { false,        false,    kBrowser,        2640,                     0, {  } },
};

const internal::SchemaNode kSchemas[] = {
//  Type                           Extra  IsSensitiveValue HasSensitiveChildren
  { base::Value::Type::DICT,           0, false,           false },  // root node
  { base::Value::Type::BOOLEAN,       -1, false,           false },  // simple type: boolean
  { base::Value::Type::STRING,        -1, false,           false },  // simple type: string
};

const internal::PropertyNode kPropertyNodes[] = {
//  Property                                                             Schema
  { key::kChunkOneFirstFieldBooleanPolicy,                                1 },
  { key::kChunkOneLastFieldBooleanPolicy,                                 1 },
  { key::kChunkTwoFirstFieldStringPolicy,                                 2 },
  { key::kChunkTwoLastFieldStringPolicy,                                  2 },
  { key::kChunkZeroLastFieldBooleanPolicy,                                1 },
  { key::kCloudManagementEnrollmentToken,                                 2 },
  { key::kCloudOnlyPolicy,                                                1 },
  { key::kExampleBoolMergeMetapolicy,                                     1 },
  { key::kExampleBoolPolicy,                                              1 },
  { key::kExampleBoolPrecedenceMetapolicy,                                1 },
  { key::kExampleStringPolicy,                                            2 },
};

const internal::PropertiesNode kProperties[] = {
//  Begin    End  PatternEnd  RequiredBegin  RequiredEnd  Additional Properties
  {     0,    11,    11,     0,          0,    -1 },  // root node
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
#if BUILDFLAG(IS_CHROMEOS)
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
  static constexpr int begin_index = 0;
  // One-past-the-end of the Chrome policies in kPropertyNodes.
  static constexpr int end_index = 11;
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
  CHECK_LT(index, std::size(kChromePolicyDetails));
  return kChromePolicyDetails + index;
}

namespace key {

const char kExampleStringPolicy[] = "ExampleStringPolicy";
const char kExampleBoolPolicy[] = "ExampleBoolPolicy";
const char kExampleBoolMergeMetapolicy[] = "ExampleBoolMergeMetapolicy";
const char kExampleBoolPrecedenceMetapolicy[] = "ExampleBoolPrecedenceMetapolicy";
const char kCloudOnlyPolicy[] = "CloudOnlyPolicy";
const char kCloudManagementEnrollmentToken[] = "CloudManagementEnrollmentToken";
const char kChunkZeroLastFieldBooleanPolicy[] = "ChunkZeroLastFieldBooleanPolicy";
const char kChunkOneFirstFieldBooleanPolicy[] = "ChunkOneFirstFieldBooleanPolicy";
const char kChunkOneLastFieldBooleanPolicy[] = "ChunkOneLastFieldBooleanPolicy";
const char kChunkTwoFirstFieldStringPolicy[] = "ChunkTwoFirstFieldStringPolicy";
const char kChunkTwoLastFieldStringPolicy[] = "ChunkTwoLastFieldStringPolicy";

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

const std::array<BooleanPolicyAccess, 7> kBooleanPolicyAccess {{
  {key::kExampleBoolPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_exampleboolpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.exampleboolpolicy();
   }
  },
  {key::kExampleBoolMergeMetapolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_exampleboolmergemetapolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.exampleboolmergemetapolicy();
   }
  },
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_exampleboolprecedencemetapolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.exampleboolprecedencemetapolicy();
   }
  },
  {key::kCloudOnlyPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_cloudonlypolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.cloudonlypolicy();
   }
  },
  {key::kChunkZeroLastFieldBooleanPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_chunkzerolastfieldbooleanpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.chunkzerolastfieldbooleanpolicy();
   }
  },
  {key::kChunkOneFirstFieldBooleanPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_subproto1() &&
              policy.subproto1().has_chunkonefirstfieldbooleanpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.subproto1().chunkonefirstfieldbooleanpolicy();
   }
  },
  {key::kChunkOneLastFieldBooleanPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_subproto1() &&
              policy.subproto1().has_chunkonelastfieldbooleanpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::BooleanPolicyProto& {
     return policy.subproto1().chunkonelastfieldbooleanpolicy();
   }
  },
}};

const std::array<IntegerPolicyAccess, 0> kIntegerPolicyAccess {{
}};

const std::array<StringPolicyAccess, 4> kStringPolicyAccess {{
  {key::kExampleStringPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_examplestringpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::StringPolicyProto& {
     return policy.examplestringpolicy();
   },
   StringPolicyType::STRING
  },
  {key::kCloudManagementEnrollmentToken,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_cloudmanagementenrollmenttoken();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::StringPolicyProto& {
     return policy.cloudmanagementenrollmenttoken();
   },
   StringPolicyType::STRING
  },
  {key::kChunkTwoFirstFieldStringPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_subproto2() &&
              policy.subproto2().has_chunktwofirstfieldstringpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::StringPolicyProto& {
     return policy.subproto2().chunktwofirstfieldstringpolicy();
   },
   StringPolicyType::STRING
  },
  {key::kChunkTwoLastFieldStringPolicy,
   false,
   [](const em::CloudPolicySettings& policy) {
     return policy.has_subproto2() &&
              policy.subproto2().has_chunktwolastfieldstringpolicy();
   },
   [](const em::CloudPolicySettings& policy)
       -> const em::StringPolicyProto& {
     return policy.subproto2().chunktwolastfieldstringpolicy();
   },
   StringPolicyType::STRING
  },
}};

const std::array<StringListPolicyAccess, 0> kStringListPolicyAccess {{
}};


}  // namespace policy
'''

POLICY_CONSTANTS_SOURCE_WIN_ONLY_PART = '''
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Google\\\\Chrome";
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Google\\\\Chrome for Testing";
#else
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Chromium";
#endif
'''

EXPECTED_CROS_POLICY_CONSTANTS_HEADER = '''
#ifndef __BINDINGS_POLICY_CONSTANTS_H_
#define __BINDINGS_POLICY_CONSTANTS_H_

#include <array>

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
extern const char kCloudManagementEnrollmentToken[];
extern const char kChunkZeroLastFieldBooleanPolicy[];
extern const char kChunkOneFirstFieldBooleanPolicy[];
extern const char kChunkOneLastFieldBooleanPolicy[];
extern const char kChunkTwoFirstFieldStringPolicy[];
extern const char kChunkTwoLastFieldStringPolicy[];

}  // namespace key

// NULL-terminated list of device policy registry key names.
extern const char* kDevicePolicyKeys[];

// Access to the mutable protobuf function of all supported boolean user
// policies.
struct BooleanPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::BooleanPolicyProto* (*mutable_proto_ptr)(
      enterprise_management::CloudPolicySettings* policy);
};
extern const std::array<BooleanPolicyAccess, 7> kBooleanPolicyAccess;

// Access to the mutable protobuf function of all supported integer user
// policies.
struct IntegerPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::IntegerPolicyProto* (*mutable_proto_ptr)(
      enterprise_management::CloudPolicySettings* policy);
};
extern const std::array<IntegerPolicyAccess, 0> kIntegerPolicyAccess;

// Access to the mutable protobuf function of all supported string user
// policies.
struct StringPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringPolicyProto* (*mutable_proto_ptr)(
      enterprise_management::CloudPolicySettings* policy);
};
extern const std::array<StringPolicyAccess, 4> kStringPolicyAccess;

// Access to the mutable protobuf function of all supported stringlist user
// policies.
struct StringListPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringListPolicyProto* (*mutable_proto_ptr)(
      enterprise_management::CloudPolicySettings* policy);
};
extern const std::array<StringListPolicyAccess, 0> kStringListPolicyAccess;

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
const char kCloudManagementEnrollmentToken[] = "CloudManagementEnrollmentToken";
const char kChunkZeroLastFieldBooleanPolicy[] = "ChunkZeroLastFieldBooleanPolicy";
const char kChunkOneFirstFieldBooleanPolicy[] = "ChunkOneFirstFieldBooleanPolicy";
const char kChunkOneLastFieldBooleanPolicy[] = "ChunkOneLastFieldBooleanPolicy";
const char kChunkTwoFirstFieldStringPolicy[] = "ChunkTwoFirstFieldStringPolicy";
const char kChunkTwoLastFieldStringPolicy[] = "ChunkTwoLastFieldStringPolicy";

}  // namespace key

const char* kDevicePolicyKeys[] = {

  nullptr};

const std::array<BooleanPolicyAccess, 7> kBooleanPolicyAccess {{
  {key::kExampleBoolPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_exampleboolpolicy();
   }
  },
  {key::kExampleBoolMergeMetapolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_exampleboolmergemetapolicy();
   }
  },
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_exampleboolprecedencemetapolicy();
   }
  },
  {key::kCloudOnlyPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_cloudonlypolicy();
   }
  },
  {key::kChunkZeroLastFieldBooleanPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_chunkzerolastfieldbooleanpolicy();
   }
  },
  {key::kChunkOneFirstFieldBooleanPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_subproto1()->mutable_chunkonefirstfieldbooleanpolicy();
   }
  },
  {key::kChunkOneLastFieldBooleanPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::BooleanPolicyProto* {
     return policy->mutable_subproto1()->mutable_chunkonelastfieldbooleanpolicy();
   }
  },
}};

const std::array<IntegerPolicyAccess, 0> kIntegerPolicyAccess {{
}};

const std::array<StringPolicyAccess, 4> kStringPolicyAccess {{
  {key::kExampleStringPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::StringPolicyProto* {
     return policy->mutable_examplestringpolicy();
   }
  },
  {key::kCloudManagementEnrollmentToken,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::StringPolicyProto* {
     return policy->mutable_cloudmanagementenrollmenttoken();
   }
  },
  {key::kChunkTwoFirstFieldStringPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::StringPolicyProto* {
     return policy->mutable_subproto2()->mutable_chunktwofirstfieldstringpolicy();
   }
  },
  {key::kChunkTwoLastFieldStringPolicy,
   false,
   [](em::CloudPolicySettings* policy)
       -> em::StringPolicyProto* {
     return policy->mutable_subproto2()->mutable_chunktwolastfieldstringpolicy();
   }
  },
}};

const std::array<StringListPolicyAccess, 0> kStringListPolicyAccess {{
}};

}  // namespace policy
'''

EXPECTED_APP_RESTRICTIONS_XML = '''
<restrictions xmlns:android="http://schemas.android.com/apk/res/android">

    <restriction
        android:key="CloudManagementEnrollmentToken"
        android:title="@string/CloudManagementEnrollmentTokenTitle"
        android:description="@string/CloudManagementEnrollmentTokenDesc"
        android:restrictionType="string"/>

    <restriction
        android:key="ChunkOneFirstFieldBooleanPolicy"
        android:title="@string/ChunkOneFirstFieldBooleanPolicyTitle"
        android:description="@string/ChunkOneFirstFieldBooleanPolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ChunkOneLastFieldBooleanPolicy"
        android:title="@string/ChunkOneLastFieldBooleanPolicyTitle"
        android:description="@string/ChunkOneLastFieldBooleanPolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ChunkTwoFirstFieldStringPolicy"
        android:title="@string/ChunkTwoFirstFieldStringPolicyTitle"
        android:description="@string/ChunkTwoFirstFieldStringPolicyDesc"
        android:restrictionType="string"/>

    <restriction
        android:key="ChunkTwoLastFieldStringPolicy"
        android:title="@string/ChunkTwoLastFieldStringPolicyTitle"
        android:description="@string/ChunkTwoLastFieldStringPolicyDesc"
        android:restrictionType="string"/>

    <restriction
        android:key="ChunkZeroLastFieldBooleanPolicy"
        android:title="@string/ChunkZeroLastFieldBooleanPolicyTitle"
        android:description="@string/ChunkZeroLastFieldBooleanPolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleBoolMergeMetapolicy"
        android:title="@string/ExampleBoolMergeMetapolicyTitle"
        android:description="@string/ExampleBoolMergeMetapolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleBoolPolicy"
        android:title="@string/ExampleBoolPolicyTitle"
        android:description="@string/ExampleBoolPolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleBoolPrecedenceMetapolicy"
        android:title="@string/ExampleBoolPrecedenceMetapolicyTitle"
        android:description="@string/ExampleBoolPrecedenceMetapolicyDesc"
        android:restrictionType="bool"/>

    <restriction
        android:key="ExampleStringPolicy"
        android:title="@string/ExampleStringPolicyTitle"
        android:description="@string/ExampleStringPolicyDesc"
        android:restrictionType="string"/>

</restrictions>'''
