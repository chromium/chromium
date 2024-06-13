// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace certificate_transparency {
namespace prefs {

// Registers the preferences related to Certificate Transparency policy
// in the given pref registry.
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY)
void RegisterPrefs(PrefRegistrySimple* registry);

// The set of hosts (as URLBlocklist-syntax filters) for which Certificate
// Transparency information is allowed to be absent, even if it would
// otherwise be required (e.g. as part of security policy).
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) extern const char kCTExcludedHosts[];

// The set of subjectPublicKeyInfo hashes in the form of
// <hash-name>"/"<base64-hash-value>. If a certificate matches this SPKI, then
// Certificate Transparency information is allowed to be absent if one of the
// following conditions are met:
// 1) The matching certificate is a CA certificate (basicConstraints CA:TRUE)
//    that has a nameConstraints extension with a permittedSubtrees that
//    contains one or more directoryName entries, the directoryName has
//    one or more organizationName attributes, and the leaf certificate also
//    contains one or more organizationName attributes in the Subject.
// 2) The matching certificate contains one or more organizationName
//    attributes in the Subject, and those attributes are identical in
//    ordering, number of values, and byte-for-byte equality of values.
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) extern const char kCTExcludedSPKIs[];

}  // namespace prefs
}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
