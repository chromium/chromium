// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_member.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/value_conversions.h"
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
    prefs_ = nullptr;
  }
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
                                         const std::string& pref_name) {
  VerifyValuePrefName();
  UpdateValueFromPref((!setting_value_ && !observer_.is_null())
                          ? base::BindOnce(observer_, pref_name)
                          : base::OnceClosure());
}

void PrefMemberBase::UpdateValueFromPref(base::OnceClosure callback) const {
  VerifyValuePrefName();
  const PrefService::Preference* pref = prefs_->FindPreference(pref_name_);
  DCHECK(pref);
  if (!internal())
    CreateInternal();
  internal()->UpdateValue(pref->GetValue()->DeepCopy(), pref->IsManaged(),
                          pref->IsUserModifiable(), std::move(callback));
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
    : owning_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      is_managed_(false),
      is_user_modifiable_(false) {}
PrefMemberBase::Internal::~Internal() { }

bool PrefMemberBase::Internal::IsOnCorrectSequence() const {
  return owning_task_runner_->RunsTasksInCurrentSequence();
}

void PrefMemberBase::Internal::UpdateValue(base::Value* v,
                                           bool is_managed,
                                           bool is_user_modifiable,
                                           base::OnceClosure callback) const {
  std::unique_ptr<base::Value> value(v);
  base::ScopedClosureRunner closure_runner(std::move(callback));
  if (IsOnCorrectSequence()) {
    bool rv = UpdateValueInternal(*value);
    DCHECK(rv);
    is_managed_ = is_managed;
    is_user_modifiable_ = is_user_modifiable;
  } else {
    bool may_run = owning_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PrefMemberBase::Internal::UpdateValue, this,
                       value.release(), is_managed, is_user_modifiable,
                       closure_runner.Release()));
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
  const base::ListValue* list = static_cast<const base::ListValue*>(&value);

  std::vector<std::string> local_vector;
  for (auto it = list->begin(); it != list->end(); ++it) {
    std::string string_value;
    if (!it->GetAsString(&string_value))
      return false;

    local_vector.push_back(string_value);
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
  return value.GetAsBoolean(&value_);
}

template <>
void PrefMember<int>::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name(), value);
}

template <>
bool PrefMember<int>::Internal::UpdateValueInternal(
    const base::Value& value) const {
  return value.GetAsInteger(&value_);
}

template <>
void PrefMember<double>::UpdatePref(const double& value) {
  prefs()->SetDouble(pref_name(), value);
}

template <>
bool PrefMember<double>::Internal::UpdateValueInternal(const base::Value& value)
    const {
  return value.GetAsDouble(&value_);
}

template <>
void PrefMember<std::string>::UpdatePref(const std::string& value) {
  prefs()->SetString(pref_name(), value);
}

template <>
bool PrefMember<std::string>::Internal::UpdateValueInternal(
    const base::Value& value)
    const {
  return value.GetAsString(&value_);
}

template <>
void PrefMember<base::FilePath>::UpdatePref(const base::FilePath& value) {
  prefs()->SetFilePath(pref_name(), value);
}

template <>
bool PrefMember<base::FilePath>::Internal::UpdateValueInternal(
    const base::Value& value)
    const {
  return base::GetValueAsFilePath(value, &value_);
}

template <>
void PrefMember<std::vector<std::string> >::UpdatePref(
    const std::vector<std::string>& value) {
  base::ListValue list_value;
  list_value.AppendStrings(value);
  prefs()->Set(pref_name(), list_value);
}

template <>
bool PrefMember<std::vector<std::string> >::Internal::UpdateValueInternal(
    const base::Value& value) const {
  return subtle::PrefMemberVectorStringUpdate(value, &value_);
}
