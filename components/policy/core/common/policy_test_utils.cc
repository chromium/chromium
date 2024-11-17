// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_test_utils.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_constants.h"

#if BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#endif

namespace policy {

PolicyDetailsMap::PolicyDetailsMap() = default;

PolicyDetailsMap::~PolicyDetailsMap() = default;

GetChromePolicyDetailsCallback PolicyDetailsMap::GetCallback() const {
  return base::BindRepeating(&PolicyDetailsMap::Lookup, base::Unretained(this));
}

void PolicyDetailsMap::SetDetails(const std::string& policy,
                                  const PolicyDetails* details) {
  map_[policy] = details;
}

const PolicyDetails* PolicyDetailsMap::Lookup(const std::string& policy) const {
  auto it = map_.find(policy);
  return it == map_.end() ? NULL : it->second.get();
}

bool PolicyServiceIsEmpty(const PolicyService* service) {
  const PolicyMap& map = service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  if (!map.empty()) {
    base::Value::Dict dict;
    for (const auto& it : map)
      dict.Set(it.first, it.second.value_unsafe()->Clone());
    LOG(WARNING) << "There are pre-existing policies in this machine: " << dict;
#if BUILDFLAG(IS_WIN)
    LOG(WARNING) << "From: " << kRegistryChromePolicyKey;
#endif
  }
  return map.empty();
}

#if BUILDFLAG(IS_APPLE)
base::apple::ScopedCFTypeRef<CFPropertyListRef> ValueToProperty(
    const base::Value& value) {
  base::apple::ScopedCFTypeRef<CFPropertyListRef> result;

  switch (value.type()) {
    case base::Value::Type::NONE: {
      result.reset(kCFNull, base::scoped_policy::RETAIN);
      break;
    }

    case base::Value::Type::BOOLEAN: {
      result.reset(value.GetBool() ? kCFBooleanTrue : kCFBooleanFalse,
                   base::scoped_policy::RETAIN);
      break;
    }

    case base::Value::Type::INTEGER: {
      const int int_value = value.GetInt();
      result.reset(
          CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &int_value));
      break;
    }

    case base::Value::Type::DOUBLE: {
      const double double_value = value.GetDouble();
      result.reset(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType,
                                  &double_value));
      break;
    }

    case base::Value::Type::STRING: {
      const std::string& string_value = value.GetString();
      result = base::SysUTF8ToCFStringRef(string_value);
      break;
    }

    case base::Value::Type::DICT: {
      const base::Value::Dict& value_dict = value.GetDict();
      base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> cf_dict(
          CFDictionaryCreateMutable(kCFAllocatorDefault, value_dict.size(),
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks));
      for (const auto [dict_key, dict_value] : value_dict) {
        base::apple::ScopedCFTypeRef<CFStringRef> cf_key =
            base::SysUTF8ToCFStringRef(dict_key);
        base::apple::ScopedCFTypeRef<CFPropertyListRef> cf_value =
            ValueToProperty(dict_value);
        if (cf_value) {
          CFDictionaryAddValue(cf_dict.get(), cf_key.get(), cf_value.get());
        }
      }
      result = cf_dict;
      break;
    }

    case base::Value::Type::LIST: {
      const base::Value::List& list = value.GetList();
      base::apple::ScopedCFTypeRef<CFMutableArrayRef> cf_array(
          CFArrayCreateMutable(kCFAllocatorDefault, list.size(),
                               &kCFTypeArrayCallBacks));
      for (const base::Value& entry : list) {
        base::apple::ScopedCFTypeRef<CFPropertyListRef> cf_value =
            ValueToProperty(entry);
        if (cf_value) {
          CFArrayAppendValue(cf_array.get(), cf_value.get());
        }
      }
      result = cf_array;
      break;
    }

    case base::Value::Type::BINARY:
      // This type isn't converted (though it can be represented as CFData)
      // because there's no equivalent JSON type, and policy values can only
      // take valid JSON values.
      break;
  }

  return result;
}
#endif  // BUILDFLAG(IS_APPLE)

std::ostream& operator<<(std::ostream& os, const PolicyBundle& bundle) {
  os << "{" << std::endl;
  for (const auto& entry : bundle)
    os << "  \"" << entry.first << "\": " << entry.second << "," << std::endl;
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, PolicyScope scope) {
  switch (scope) {
    case POLICY_SCOPE_USER:
      return os << "POLICY_SCOPE_USER";
    case POLICY_SCOPE_MACHINE:
      return os << "POLICY_SCOPE_MACHINE";
  }
  return os << "POLICY_SCOPE_UNKNOWN(" << int(scope) << ")";
}

std::ostream& operator<<(std::ostream& os, PolicyLevel level) {
  switch (level) {
    case POLICY_LEVEL_RECOMMENDED:
      return os << "POLICY_LEVEL_RECOMMENDED";
    case POLICY_LEVEL_MANDATORY:
      return os << "POLICY_LEVEL_MANDATORY";
  }
  return os << "POLICY_LEVEL_UNKNOWN(" << int(level) << ")";
}

std::ostream& operator<<(std::ostream& os, PolicyDomain domain) {
  switch (domain) {
    case POLICY_DOMAIN_CHROME:
      return os << "POLICY_DOMAIN_CHROME";
    case POLICY_DOMAIN_EXTENSIONS:
      return os << "POLICY_DOMAIN_EXTENSIONS";
    case POLICY_DOMAIN_SIGNIN_EXTENSIONS:
      return os << "POLICY_DOMAIN_SIGNIN_EXTENSIONS";
    case POLICY_DOMAIN_SIZE:
      break;
  }
  return os << "POLICY_DOMAIN_UNKNOWN(" << int(domain) << ")";
}

std::ostream& operator<<(std::ostream& os, const PolicyMap& policies) {
  os << "{" << std::endl;
  for (const auto& iter : policies)
    os << "  \"" << iter.first << "\": " << iter.second << "," << std::endl;
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const PolicyMap::Entry& e) {
  return os << "{" << std::endl
            << "  \"level\": " << e.level << "," << std::endl
            << "  \"scope\": " << e.scope << "," << std::endl
            << "  \"value\": " << *e.value_unsafe() << "}";
}

std::ostream& operator<<(std::ostream& os, const PolicyNamespace& ns) {
  return os << ns.domain << "/" << ns.component_id;
}

}  // namespace policy
