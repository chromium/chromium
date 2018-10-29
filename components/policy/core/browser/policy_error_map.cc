// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {

class PolicyErrorMap::PendingError {
 public:
  explicit PendingError(const std::string& policy_name)
      : policy_name_(policy_name) {}
  virtual ~PendingError() {}

  const std::string& policy_name() const { return policy_name_; }

  virtual base::string16 GetMessage() const = 0;

 private:
  std::string policy_name_;

  DISALLOW_COPY_AND_ASSIGN(PendingError);
};

namespace {

class SimplePendingError : public PolicyErrorMap::PendingError {
 public:
  SimplePendingError(const std::string& policy_name,
                     int message_id,
                     const std::string& replacement)
      : PendingError(policy_name),
        message_id_(message_id),
        replacement_(replacement) {}
  ~SimplePendingError() override {}

  base::string16 GetMessage() const override {
    if (message_id_ >= 0) {
      if (replacement_.empty())
        return l10n_util::GetStringUTF16(message_id_);
      return l10n_util::GetStringFUTF16(message_id_,
                                        base::ASCIIToUTF16(replacement_));
    }
    return base::ASCIIToUTF16(replacement_);
  }

 private:
  int message_id_;
  std::string replacement_;

  DISALLOW_COPY_AND_ASSIGN(SimplePendingError);
};

class DictSubkeyPendingError : public SimplePendingError {
 public:
  DictSubkeyPendingError(const std::string& policy_name,
                         const std::string& subkey,
                         int message_id,
                         const std::string& replacement)
      : SimplePendingError(policy_name, message_id, replacement),
        subkey_(subkey) {}
  ~DictSubkeyPendingError() override {}

  base::string16 GetMessage() const override {
    return l10n_util::GetStringFUTF16(IDS_POLICY_SUBKEY_ERROR,
                                      base::ASCIIToUTF16(subkey_),
                                      SimplePendingError::GetMessage());
  }

 private:
  std::string subkey_;

  DISALLOW_COPY_AND_ASSIGN(DictSubkeyPendingError);
};

class ListItemPendingError : public SimplePendingError {
 public:
  ListItemPendingError(const std::string& policy_name,
                       int index,
                       int message_id,
                       const std::string& replacement)
      : SimplePendingError(policy_name, message_id, replacement),
        index_(index) {}
  ~ListItemPendingError() override {}

  base::string16 GetMessage() const override {
    return l10n_util::GetStringFUTF16(IDS_POLICY_LIST_ENTRY_ERROR,
                                      base::IntToString16(index_),
                                      SimplePendingError::GetMessage());
  }

 private:
  int index_;

  DISALLOW_COPY_AND_ASSIGN(ListItemPendingError);
};

class SchemaValidatingPendingError : public SimplePendingError {
 public:
  SchemaValidatingPendingError(const std::string& policy_name,
                               const std::string& error_path,
                               const std::string& replacement)
      : SimplePendingError(policy_name, -1, replacement),
        error_path_(error_path) {}
  ~SchemaValidatingPendingError() override {}

  base::string16 GetMessage() const override {
    return l10n_util::GetStringFUTF16(IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                                      base::ASCIIToUTF16(error_path_),
                                      SimplePendingError::GetMessage());
  }

 private:
  std::string error_path_;

  DISALLOW_COPY_AND_ASSIGN(SchemaValidatingPendingError);
};

}  // namespace

PolicyErrorMap::PolicyErrorMap() {
}

PolicyErrorMap::~PolicyErrorMap() {
}

bool PolicyErrorMap::IsReady() const {
  return ui::ResourceBundle::HasSharedInstance();
}

void PolicyErrorMap::AddError(const std::string& policy, int message_id) {
  AddError(
      std::make_unique<SimplePendingError>(policy, message_id, std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id) {
  AddError(std::make_unique<DictSubkeyPendingError>(policy, subkey, message_id,
                                                    std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id) {
  AddError(std::make_unique<ListItemPendingError>(policy, index, message_id,
                                                  std::string()));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int message_id,
                              const std::string& replacement) {
  AddError(
      std::make_unique<SimplePendingError>(policy, message_id, replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& subkey,
                              int message_id,
                              const std::string& replacement) {
  AddError(std::make_unique<DictSubkeyPendingError>(policy, subkey, message_id,
                                                    replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              int index,
                              int message_id,
                              const std::string& replacement) {
  AddError(std::make_unique<ListItemPendingError>(policy, index, message_id,
                                                  replacement));
}

void PolicyErrorMap::AddError(const std::string& policy,
                              const std::string& error_path,
                              const std::string& message) {
  AddError(std::make_unique<SchemaValidatingPendingError>(policy, error_path,
                                                          message));
}

base::string16 PolicyErrorMap::GetErrors(const std::string& policy) {
  CheckReadyAndConvert();
  std::pair<const_iterator, const_iterator> range = map_.equal_range(policy);
  std::vector<base::StringPiece16> list;
  for (auto it = range.first; it != range.second; ++it)
    list.push_back(it->second);
  return base::JoinString(list, base::ASCIIToUTF16("\n"));
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
  debug_infos_.clear();
}

void PolicyErrorMap::SetDebugInfo(const std::string& policy,
                                  const std::string& debug_infos) {
  debug_infos_[policy] = debug_infos;
}

const std::string PolicyErrorMap::GetDebugInfo(const std::string& policy) {
  auto it = debug_infos_.find(policy);
  if (it != debug_infos_.end())
    return it->second;
  return std::string();
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
