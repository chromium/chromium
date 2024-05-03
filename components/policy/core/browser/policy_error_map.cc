// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/schema.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

namespace {

std::u16string ConvertReplacementToUTF16(const std::string& replacement) {
  if (!base::IsStringUTF8(replacement))
    return u"<invalid Unicode string>";
  return base::UTF8ToUTF16(replacement);
}

}  // namespace

bool PolicyErrorMap::Data::operator==(const PolicyErrorMap::Data& other) const {
  return std::tie(message, level) == std::tie(other.message, other.level);
}

class PolicyErrorMap::PendingError {
 public:
  PendingError(const std::string& policy_name,
               int message_id,
               std::vector<std::string> replacements,
               const PolicyErrorPath& error_path,
               const PolicyMap::MessageType level)
      : policy_name_(policy_name),
        message_id_(message_id),
        replacements_(std::move(replacements)),
        error_path_string_(ErrorPathToString(policy_name, error_path)),
        level_(level) {
    DCHECK(!base::ranges::any_of(replacements_, &std::string::empty));
  }
  PendingError(const PendingError&) = delete;
  PendingError& operator=(const PendingError&) = delete;
  ~PendingError() = default;

  const std::string& policy_name() const { return policy_name_; }

  const PolicyMap::MessageType& level() const { return level_; }

  std::u16string GetMessage() const {
    if (error_path_string_.empty())
      return GetMessageContent();
    return l10n_util::GetStringFUTF16(IDS_POLICY_ERROR_WITH_PATH,
                                      base::ASCIIToUTF16(error_path_string_),
                                      GetMessageContent());
  }

  std::u16string GetMessageContent() const {
    // TODO(crbug.com/40832324): remove this together with
    // AddError(policy, message, error_path) and add a DCHECK
    if (message_id_ >= 0) {
      std::vector<std::u16string> utf_16_replacements;
      base::ranges::transform(replacements_,
                              std::back_inserter(utf_16_replacements),
                              &ConvertReplacementToUTF16);
      return l10n_util::GetStringFUTF16(message_id_, utf_16_replacements,
                                        nullptr);
    }
    if (!replacements_.empty()) {
      return ConvertReplacementToUTF16(replacements_.front());
    }
    return std::u16string();
  }

 private:
  std::string policy_name_;
  int message_id_;
  std::vector<std::string> replacements_;
  std::string error_path_string_;
  PolicyMap::MessageType level_;
};

PolicyErrorMap::PolicyErrorMap() = default;

PolicyErrorMap::~PolicyErrorMap() = default;

bool PolicyErrorMap::IsReady() const {
  return ui::ResourceBundle::HasSharedInstance();
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              PolicyErrorPath error_path,
                              PolicyMap::MessageType level) {
  AddError(policy, message_id, std::vector<std::string>(), error_path, level);
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement,
                              PolicyErrorPath error_path,
                              PolicyMap::MessageType level) {
  AddError(policy, message_id, std::vector<std::string>{replacement},
           error_path, level);
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement_a,
                              const std::string& replacement_b,
                              PolicyErrorPath error_path,
                              PolicyMap::MessageType level) {
  AddError(policy, message_id,
           std::vector<std::string>{replacement_a, replacement_b}, error_path,
           level);
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              std::vector<std::string> replacements,
                              PolicyErrorPath error_path,
                              PolicyMap::MessageType level) {
  AddError(std::make_unique<PendingError>(
      policy, message_id, std::move(replacements), error_path, level));
}

bool PolicyErrorMap::HasError(const std::string& policy) {
  if (IsReady()) {
    CheckReadyAndConvert();
    return base::Contains(map_, policy);
  }
  return base::Contains(pending_, policy, &PendingError::policy_name);
}

bool PolicyErrorMap::HasFatalError(const std::string& policy) {
  std::pair<std::string, PolicyMap::MessageType> fatal_error =
      std::make_pair(policy, PolicyMap::MessageType::kError);
  if (IsReady()) {
    CheckReadyAndConvert();
    return base::Contains(
        map_, fatal_error, [](const std::pair<std::string, Data>& entry) {
          return std::make_pair(entry.first, entry.second.level);
        });
  }
  return base::Contains(pending_, fatal_error, [](const auto& entry) {
    return std::make_pair(entry->policy_name(), entry->level());
  });
}

std::u16string PolicyErrorMap::GetErrorMessages(const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<std::u16string_view> list;
  for (auto it = range.first; it != range.second; ++it)
    list.push_back(it->second.message);
  return base::JoinString(list, u"\n");
}

std::vector<PolicyErrorMap::Data> PolicyErrorMap::GetErrors(
    const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<PolicyErrorMap::Data> list;
  for (auto it = range.first; it != range.second; ++it)
    list.push_back(it->second);
  return list;
}

bool PolicyErrorMap::empty() const {
  // This doesn't call CheckReadyAndConvert() to allow code to destroy empty
  // PolicyErrorMaps rather than having to wait for ResourceBundle to be ready.
  return pending_.empty() && map_.empty();
}

size_t PolicyErrorMap::size() {
  CheckReadyAndConvert();
  return map_.size();
}

PolicyErrorMap::const_iterator PolicyErrorMap::begin() {
  CheckReadyAndConvert();
  return map_.begin();
}

PolicyErrorMap::const_iterator PolicyErrorMap::end() {
  CheckReadyAndConvert();
  return map_.end();
}

void PolicyErrorMap::Clear() {
  CheckReadyAndConvert();
  map_.clear();
}

void PolicyErrorMap::AddError(std::unique_ptr<PendingError> error) {
  if (IsReady()) {
    Convert(error.get());
    // Allow error to be deleted as it exits function scope.
  } else {
    pending_.push_back(std::move(error));
  }
}

void PolicyErrorMap::Convert(PendingError* error) {
  map_.insert(std::make_pair(
      error->policy_name(),
      Data{.message = error->GetMessage(), .level = error->level()}));
}

void PolicyErrorMap::CheckReadyAndConvert() {
  DCHECK(IsReady());
  for (size_t i = 0; i < pending_.size(); ++i) {
    Convert(pending_[i].get());
  }
  pending_.clear();
}

}  // namespace policy
