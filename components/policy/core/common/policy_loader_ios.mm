// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#import <UIKit/UIKit.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

PolicyLoaderIOS::PolicyLoaderIOS(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/true),
      weak_factory_(this) {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  policy_schema_ = registry->schema_map()->GetSchema(ns);
}

PolicyLoaderIOS::~PolicyLoaderIOS() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
}

void PolicyLoaderIOS::InitOnBackgroundThread() {}

PolicyBundle PolicyLoaderIOS::Load() {
  PolicyBundle bundle;
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  LoadNSDictionaryToPolicyBundle(configuration, &bundle);

  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  size_t count = bundle.Get(chrome_ns).size();
  UMA_HISTOGRAM_COUNTS_100("Enterprise.IOSPolicies", count);

  return bundle;
}

void PolicyLoaderIOS::LoadNSDictionaryToPolicyBundle(NSDictionary* dictionary,
                                                     PolicyBundle* bundle) {
  // NSDictionary is toll-free bridged to CFDictionaryRef, which is a
  // CFPropertyListRef.
  std::unique_ptr<base::Value> value =
      PropertyToValue((__bridge CFPropertyListRef)(dictionary));
  if (!value || !value->is_dict())
    return;

  PolicyMap& map = bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  for (const auto it : value->GetDict()) {
    map.Set(it.first, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
            POLICY_SOURCE_PLATFORM,
            ConvertPolicyDataIfNecessary(it.first, it.second), nullptr);
  }
}

base::Value PolicyLoaderIOS::ConvertPolicyDataIfNecessary(
    const std::string& key,
    const base::Value& value) {
  const Schema schema = policy_schema_->GetKnownProperty(key);

  if (!schema.valid()) {
    return value.Clone();
  }

  // Handle the case of a JSON-encoded string for a dict policy.
  if ((schema.type() == base::Value::Type::DICT ||
       schema.type() == base::Value::Type::LIST) &&
      value.is_string()) {
    absl::optional<base::Value> decoded_value = base::JSONReader::Read(
        value.GetString(), base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (decoded_value.has_value()) {
      return std::move(decoded_value.value());
    }
  }

  // Otherwise return an unchanged value.
  return value.Clone();
}

}  // namespace policy
