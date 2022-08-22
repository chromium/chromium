// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/schema.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

class PolicyErrorMap::PendingError {
 public:
  PendingError(const std::string& policy_name,
               int message_id,
               const PolicyErrorPath& error_path)
      : PendingError(policy_name,
                     message_id,
                     std::string(),
                     std::string(),
                     error_path) {}
  PendingError(const std::string& policy_name,
               int message_id,
               const std::string& replacement_a,
               const PolicyErrorPath& error_path)
      : PendingError(policy_name,
                     message_id,
                     replacement_a,
                     std::string(),
                     error_path) {}

  PendingError(const std::string& policy_name,
               int message_id,
               const std::string& replacement_a,
               const std::string& replacement_b,
               const PolicyErrorPath& error_path)
      : policy_name_(policy_name),
        message_id_(message_id),
        replacement_a_(replacement_a),
        replacement_b_(replacement_b),
        error_path_string_(ErrorPathToString(policy_name, error_path)) {
    DCHECK(replacement_b.empty() || !replacement_a.empty());
  }
  PendingError(const PendingError&) = delete;
  PendingError& operator=(const PendingError&) = delete;
  ~PendingError() = default;

  const std::string& policy_name() const { return policy_name_; }

  std::u16string GetMessage() const {
    if (error_path_string_.empty())
      return GetMessageContent();
    return l10n_util::GetStringFUTF16(IDS_POLICY_ERROR_WITH_PATH,
                                      base::ASCIIToUTF16(error_path_string_),
                                      GetMessageContent());
  }

  std::u16string GetMessageContent() const {
    // TODO(crbug.com/1313477): remove this together with
    // AddError(policy, message, error_path) and add a DCHECK
    if (message_id_ >= 0) {
      if (replacement_a_.empty() && replacement_b_.empty())
        return l10n_util::GetStringUTF16(message_id_);
      if (replacement_b_.empty()) {
        return l10n_util::GetStringFUTF16(message_id_,
                                          base::ASCIIToUTF16(replacement_a_));
      }
      return l10n_util::GetStringFUTF16(message_id_,
                                        base::ASCIIToUTF16(replacement_a_),
                                        base::ASCIIToUTF16(replacement_b_));
    }
    return base::ASCIIToUTF16(replacement_a_);
  }

 private:
  std::string policy_name_;
  int message_id_;
  std::string replacement_a_;
  std::string replacement_b_;
  std::string error_path_string_;
};

PolicyErrorMap::PolicyErrorMap() = default;

PolicyErrorMap::~PolicyErrorMap() = default;

bool PolicyErrorMap::IsReady() const {
  return ui::ResourceBundle::HasSharedInstance();
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              PolicyErrorPath error_path) {
  AddError(std::make_unique<PendingError>(policy, message_id, error_path));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement,
                              PolicyErrorPath error_path) {
  AddError(std::make_unique<PendingError>(policy, message_id, replacement,
                                          error_path));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement_a,
                              const std::string& replacement_b,
                              PolicyErrorPath error_path) {
  AddError(std::make_unique<PendingError>(policy, message_id, replacement_a,
                                          replacement_b, error_path));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& message,
                              PolicyErrorPath error_path) {
  AddError(std::make_unique<PendingError>(policy, -1, message, error_path));
}

bool PolicyErrorMap::HasError(const std::string& policy) {
  if (IsReady()) {
    CheckReadyAndConvert();
    return map_.find(policy) != map_.end();
  } else {
    return std::find_if(pending_.begin(), pending_.end(),
                        [policy](const auto& error) {
                          return error->policy_name() == policy;
                        }) != pending_.end();
  }
}

std::u16string PolicyErrorMap::GetErrors(const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<base::StringPiece16> list;
  for (auto it = range.first; it != range.second; ++it)
    list.push_back(it->second);
  return base::JoinString(list, u"\n");
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
  map_.insert(std::make_pair(error->policy_name(), error->GetMessage()));
}

void PolicyErrorMap::CheckReadyAndConvert() {
  DCHECK(IsReady());
  for (size_t i = 0; i < pending_.size(); ++i) {
    Convert(pending_[i].get());
  }
  pending_.clear();
}

}  // namespace policy
