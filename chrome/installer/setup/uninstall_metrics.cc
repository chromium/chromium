// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/uninstall_metrics.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/setup/setup_constants.h"
#include "components/metrics/metrics_pref_names.h"

namespace installer {

namespace {

// Appends an URL query parameter to |metrics| for each item in
// |uninstall_metrics_dict|. Returns true if |metrics| was modified; otherwise,
// false.
bool BuildUninstallMetricsString(const base::Value& uninstall_metrics_dict,
                                 base::string16* metrics) {
  DCHECK(uninstall_metrics_dict.is_dict());
  DCHECK(metrics);
  bool has_values = false;

  for (const auto& item : uninstall_metrics_dict.DictItems()) {
    has_values = true;
    metrics->push_back(L'&');
    metrics->append(base::UTF8ToWide(item.first));
    metrics->push_back(L'=');

    if (item.second.is_string())
      metrics->append(base::UTF8ToWide(item.second.GetString()));
    else
      NOTREACHED() << item.second.type();
  }

  return has_values;
}

}  // namespace

bool ExtractUninstallMetrics(const base::Value& root,
                             base::string16* uninstall_metrics_string) {
  DCHECK(root.is_dict());
  // Make sure that the user wants us reporting metrics. If not, don't add our
  // uninstall metrics.
  auto path =
      base::SplitStringPiece(metrics::prefs::kMetricsReportingEnabled, ".",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const auto* value = root.FindPathOfType(path, base::Value::Type::BOOLEAN);
  if (!value || !value->GetBool())
    return false;

  value = root.FindKeyOfType(installer::kUninstallMetricsName,
                             base::Value::Type::DICTIONARY);
  if (!value)
    return false;

  return BuildUninstallMetricsString(*value, uninstall_metrics_string);
}

bool ExtractUninstallMetricsFromFile(const base::FilePath& file_path,
                                     base::string16* uninstall_metrics_string) {
  JSONFileValueDeserializer json_deserializer(file_path);

  std::string json_error_string;
  std::unique_ptr<base::Value> root =
      json_deserializer.Deserialize(nullptr, nullptr);
  if (!root)
    return false;

  // Preferences should always have a dictionary root.
  if (!root->is_dict())
    return false;

  return ExtractUninstallMetrics(*root, uninstall_metrics_string);
}

}  // namespace installer
