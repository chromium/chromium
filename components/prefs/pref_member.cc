// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_member.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"

using base::SequencedTaskRunner;

namespace subtle {

PrefMemberBase::PrefMemberBase() : prefs_(nullptr), setting_value_(false) {}

PrefMemberBase::~PrefMemberBase() {
  Destroy();
}

void PrefMemberBase::Init(const std::string& pref_name,
                          PrefService* prefs,
                          const NamedChangeCallback& observer) {
  observer_ = observer;
  Init(pref_name, prefs);
}

void PrefMemberBase::Init(const std::string& pref_name, PrefService* prefs) {
  DCHECK(prefs);
  DCHECK(pref_name_.empty());  // Check that Init is only called once.
  prefs_ = prefs;
  pref_name_ = pref_name;
  // Check that the preference is registered.
  DCHECK(prefs_->FindPreference(pref_name_)) << pref_name << " not registered.";

  // Add ourselves as a pref observer so we can keep our local value in sync.
  prefs_->AddPrefObserver(pref_name, this);
}

void PrefMemberBase::Destroy() {
  if (prefs_ && !pref_name_.empty()) {
    prefs_->RemovePrefObserver(pref_name_, this);
  }
  prefs_ = nullptr;
}

void PrefMemberBase::MoveToSequence(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  VerifyValuePrefName();
  // Load the value from preferences if it hasn't been loaded so far.
  if (!internal())
    UpdateValueFromPref(base::OnceClosure());
  internal()->MoveToSequence(std::move(task_runner));
}

void PrefMemberBase::OnPreferenceChanged(PrefService* service,
                                         std::string_view pref_name) {
  VerifyValuePrefName();
  UpdateValueFromPref((!setting_value_ && !observer_.is_null())
                          ? base::BindOnce(observer_, std::string(pref_name))
                          : base::OnceClosure());
}

void PrefMemberBase::UpdateValueFromPref(base::OnceClosure callback) const {
  VerifyValuePrefName();
  const PrefService::Preference* pref = prefs_->FindPreference(pref_name_);
  DCHECK(pref);
  if (!internal())
    CreateInternal();
  internal()->UpdateValue(
      base::Value::ToUniquePtrValue(pref->GetValue()->Clone()).release(),
      pref->IsManaged(), pref->IsUserModifiable(), pref->IsDefaultValue(),
      std::move(callback));
}

void PrefMemberBase::VerifyPref() const {
  VerifyValuePrefName();
  if (!internal())
    UpdateValueFromPref(base::OnceClosure());
}

void PrefMemberBase::InvokeUnnamedCallback(
    const base::RepeatingClosure& callback,
    const std::string& pref_name) {
  callback.Run();
}

PrefMemberBase::Internal::Internal()
    : owning_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
PrefMemberBase::Internal::~Internal() = default;

bool PrefMemberBase::Internal::IsOnCorrectSequence() const {
  return owning_task_runner_->RunsTasksInCurrentSequence();
}

void PrefMemberBase::Internal::UpdateValue(base::Value* v,
                                           bool is_managed,
                                           bool is_user_modifiable,
                                           bool is_default_value,
                                           base::OnceClosure callback) const {
  std::unique_ptr<base::Value> value(v);
  base::ScopedClosureRunner closure_runner(std::move(callback));
  if (IsOnCorrectSequence()) {
    bool rv = UpdateValueInternal(*value);
    DCHECK(rv);
    is_managed_ = is_managed;
    is_user_modifiable_ = is_user_modifiable;
    is_default_value_ = is_default_value;
  } else {
    bool may_run = owning_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PrefMemberBase::Internal::UpdateValue, this,
                       value.release(), is_managed, is_user_modifiable,
                       is_default_value, closure_runner.Release()));
    DCHECK(may_run);
  }
}

void PrefMemberBase::Internal::MoveToSequence(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  CheckOnCorrectSequence();
  owning_task_runner_ = std::move(task_runner);
}

bool PrefMemberVectorStringUpdate(const base::Value& value,
                                  std::vector<std::string>* string_vector) {
  if (!value.is_list())
    return false;

  std::vector<std::string> local_vector;
  for (const auto& item : value.GetList()) {
    if (!item.is_string())
      return false;
    local_vector.push_back(item.GetString());
  }

  string_vector->swap(local_vector);
  return true;
}

}  // namespace subtle

template <>
void PrefMember<bool>::UpdatePref(const bool& value) {
  prefs()->SetBoolean(pref_name(), value);
}

template <>
bool PrefMember<bool>::Internal::UpdateValueInternal(
    const base::Value& value) const {
  if (value.is_bool())
    value_ = value.GetBool();
  return value.is_bool();
}

template <>
void PrefMember<int>::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name(), value);
}

template <>
bool PrefMember<int>::Internal::UpdateValueInternal(
    const base::Value& value) const {
  if (value.is_int())
    value_ = value.GetInt();
  return value.is_int();
}

template <>
void PrefMember<double>::UpdatePref(const double& value) {
  prefs()->SetDouble(pref_name(), value);
}

template <>
bool PrefMember<double>::Internal::UpdateValueInternal(const base::Value& value)
    const {
  if (value.is_double() || value.is_int())
    value_ = value.GetDouble();
  return value.is_double() || value.is_int();
}

template <>
void PrefMember<std::string>::UpdatePref(const std::string& value) {
  prefs()->SetString(pref_name(), value);
}

template <>
bool PrefMember<std::string>::Internal::UpdateValueInternal(
    const base::Value& value)
    const {
  if (value.is_string())
    value_ = value.GetString();
  return value.is_string();
}

template <>
void PrefMember<base::FilePath>::UpdatePref(const base::FilePath& value) {
  prefs()->SetFilePath(pref_name(), value);
}

template <>
bool PrefMember<base::FilePath>::Internal::UpdateValueInternal(
    const base::Value& value)
    const {
  std::optional<base::FilePath> path = base::ValueToFilePath(value);
  if (!path)
    return false;
  value_ = *path;
  return true;
}

template <>
void PrefMember<std::vector<std::string> >::UpdatePref(
    const std::vector<std::string>& value) {
  base::Value::List list_value;
  for (const std::string& val : value)
    list_value.Append(val);

  prefs()->SetList(pref_name(), std::move(list_value));
}

template <>
bool PrefMember<std::vector<std::string> >::Internal::UpdateValueInternal(
    const base::Value& value) const {
  return subtle::PrefMemberVectorStringUpdate(value, &value_);
}
