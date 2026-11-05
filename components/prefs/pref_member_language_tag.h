// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_MEMBER_LANGUAGE_TAG_H_
#define COMPONENTS_PREFS_PREF_MEMBER_LANGUAGE_TAG_H_

// PrefMember<base::i18n::LanguageTag>. This is separate from pref_member.h
// because base/i18n/language_tag.h is expensive to compile, and most users of
// pref_member.h don't need it.

#include "base/i18n/language_tag.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/prefs_export.h"

template <>
COMPONENTS_PREFS_EXPORT
PrefMember<base::i18n::LanguageTag>::Internal::Internal();

template <>
COMPONENTS_PREFS_EXPORT void PrefMember<base::i18n::LanguageTag>::UpdatePref(
    const base::i18n::LanguageTag& value);

template <>
COMPONENTS_PREFS_EXPORT bool
PrefMember<base::i18n::LanguageTag>::Internal::UpdateValueInternal(
    const base::Value& value) const;

typedef PrefMember<base::i18n::LanguageTag> LanguageTagPrefMember;

#endif  // COMPONENTS_PREFS_PREF_MEMBER_LANGUAGE_TAG_H_
