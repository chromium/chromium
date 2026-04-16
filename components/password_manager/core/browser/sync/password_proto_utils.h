// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace sync_pb {
class PasswordSpecifics;
class PasswordSpecificsData;
class PasswordIssues;
class PasswordSpecificsData_Notes;
}  // namespace sync_pb

namespace password_manager {

// Converts a map of `form_password_issues` into the format required by the
// proto.
sync_pb::PasswordIssues PasswordIssuesMapToProto(
    const base::flat_map<InsecureType, InsecurityMetadata>&
        form_password_issues);

// Builds a map of password issues from the proto data. The map is required
// for storing issues in a `PasswordForm`.
base::flat_map<InsecureType, InsecurityMetadata> PasswordIssuesMapFromProto(
    const sync_pb::PasswordSpecificsData& password_data);

// Converts a sync_pb::PasswordSpecificsData_Notes to a
// std::vector<PasswordNote>.
std::vector<PasswordNote> PasswordNotesFromProto(
    const sync_pb::PasswordSpecificsData_Notes& notes_proto);

// Converts a std::vector<PasswordNote> to a
// sync_pb::PasswordSpecificsData_Notes. `base_notes` is intended for carrying
// over unknown and unsupported note fields when there is a local modification
// to an existing sync entity.
sync_pb::PasswordSpecificsData_Notes PasswordNotesToProto(
    const std::vector<PasswordNote>& notes,
    const sync_pb::PasswordSpecificsData_Notes& base_notes);

// Returns sync_pb::PasswordSpecificsData based on given `credential`.
// `base_password_data` is intended for carrying over unknown and unsupported
// fields when there is a local modification to an existing sync entity. The
// resulting proto contains all supported fields from `credential` combined
// with unsupported fields from `base_password_data`
sync_pb::PasswordSpecificsData SpecificsDataFromStoredCredential(
    const StoredCredential& credential);
sync_pb::PasswordSpecificsData SpecificsDataFromStoredCredential(
    const StoredCredential& credential,
    const sync_pb::PasswordSpecificsData& base_password_data);

// Returns sync_pb::PasswordSpecifics based on given `credential`.
sync_pb::PasswordSpecifics SpecificsFromStoredCredential(
    const StoredCredential& credential);
sync_pb::PasswordSpecifics SpecificsFromStoredCredential(
    const StoredCredential& credential,
    const sync_pb::PasswordSpecificsData& base_password_data);

// Returns a partial StoredCredential for a given set of `password_data`.
StoredCredential StoredCredentialFromSpecifics(
    const sync_pb::PasswordSpecificsData& password_data);

// Returns a copy of |password_specifics_data| with cleared supported fields
// that don't need to be preserved in EntityMetadata cache.
sync_pb::PasswordSpecificsData TrimPasswordSpecificsDataForCaching(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_
