// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_manager.h"

#include "base/i18n/case_conversion.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"

using autofill::FieldRendererId;
using autofill::FormSignature;

namespace password_manager {

namespace {

bool IsSameField(const FieldInfo& lhs, const FieldInfo& rhs) {
  return lhs.driver_id == rhs.driver_id && lhs.field_id == rhs.field_id;
}

// If |predictions| contain the field identified by |field_info|, stores them in
// |field_info| and returns true.
bool StoresPredictionsForInfo(const FormPredictions& predictions,
                              FieldInfo& field_info) {
  FieldRendererId field_id = field_info.field_id;
  auto field = base::ranges::find_if(
      predictions.fields, [field_id](const PasswordFieldPrediction& field) {
        return field.renderer_id == field_id;
      });
  if (field == predictions.fields.end()) {
    return false;
  }
  field_info.stored_predictions = predictions;
  field_info.type = field->type;
  return true;
}

}  // namespace

FieldInfo::FieldInfo(int driver_id,
                     FieldRendererId field_id,
                     std::string signon_realm,
                     std::u16string value,
                     bool is_likely_otp)
    : driver_id(driver_id),
      field_id(field_id),
      signon_realm(signon_realm),
      value(base::i18n::ToLower(value)),
      is_likely_otp(is_likely_otp) {}

FieldInfo::FieldInfo(const FieldInfo&) = default;
FieldInfo& FieldInfo::operator=(const FieldInfo&) = default;

FieldInfo::~FieldInfo() = default;

FieldInfoManager::FieldInfoManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

FieldInfoManager::~FieldInfoManager() = default;

void FieldInfoManager::AddFieldInfo(
    const FieldInfo& new_info,
    const std::optional<FormPredictions>& predictions) {
  if (!field_info_cache_.empty() &&
      IsSameField(field_info_cache_.back().field_info, new_info)) {
    // The method can be called on every keystroke while the user modifies
    // the field, update the value.
    field_info_cache_.back().field_info.value = new_info.value;
  } else {
    // Only the last two fields are cached to allow for one possible username
    // and one OTP/captcha field.
    if (field_info_cache_.size() >= 2) {
      ClearOldestFieldInfoEntry();
    }

    auto timer = std::make_unique<base::OneShotTimer>();
    timer->SetTaskRunner(task_runner_);
    field_info_cache_.emplace_back(new_info, std::move(timer));
  }

  // Safe to use "this", because the timer will be destructed before "this" is
  // destructed.
  field_info_cache_.back().timer->Start(
      FROM_HERE, kSingleUsernameTimeToLive, this,
      &FieldInfoManager::ClearOldestFieldInfoEntry);

  FieldInfo& field_info = field_info_cache_.back().field_info;
  if (predictions.has_value() && !field_info.stored_predictions.has_value()) {
    StoresPredictionsForInfo(predictions.value(), field_info);
  }
}

std::vector<FieldInfo> FieldInfoManager::GetFieldInfo(
    const std::string& signon_realm) {
  std::vector<FieldInfo> relevant_info;
  for (const auto& entry : field_info_cache_) {
    // TODO(crbug.com/40277063): Consider affiliated matches and PSL extension
    // list.
    if (IsPublicSuffixDomainMatch(entry.field_info.signon_realm,
                                  signon_realm)) {
      relevant_info.push_back(entry.field_info);
    }
  }
  return relevant_info;
}

void FieldInfoManager::ProcessServerPredictions(
    const std::map<autofill::FormSignature, FormPredictions>& predictions) {
  for (auto& entry : field_info_cache_) {
    FieldInfo& field_info = entry.field_info;
    // Do nothing if predictions are already stored.
    if (field_info.stored_predictions.has_value()) {
      continue;
    }

    for (const auto& prediction : predictions) {
      // Do nothing if drivers do not match.
      if (field_info.driver_id != prediction.second.driver_id) {
        continue;
      }
      if (StoresPredictionsForInfo(prediction.second, field_info)) {
        break;
      }
    }
  }
}

FieldInfoManager::FieldInfoEntry::FieldInfoEntry(
    FieldInfo field_info,
    std::unique_ptr<base::OneShotTimer> timer)
    : field_info(field_info), timer(std::move(timer)) {}

FieldInfoManager::FieldInfoEntry::~FieldInfoEntry() = default;

void FieldInfoManager::ClearOldestFieldInfoEntry() {
  field_info_cache_.pop_front();
}

}  // namespace password_manager
