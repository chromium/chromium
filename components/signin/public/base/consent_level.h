// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_CONSENT_LEVEL_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_CONSENT_LEVEL_H_

namespace signin {

// ConsentLevel is the required level of user consent for an identity operation
// (for example to fetch an OAuth2 access token).
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.identitymanager
enum class ConsentLevel {
  // No specific consent required. In particular, browser sync consent is not
  // required. Operations are allowed if the user is signed in to Chrome. It is
  // the responsibility of the client to ask for explicit user consent for any
  // operation that requires information from the primary account.
  // See "unconsented primary account" in ./README.md.
  kSignin,

  // This value is deprecated and will be removed in multiple steps. Each
  // platform will be migrated separately: iOS is expected to be migrated first,
  // Android will follow soon, while Desktop/ChromeOS might take a bit longer.
  // Expected migration phases:
  // 1. Pre-migration state: sync opt-in flows move IdentityManager into kSync
  //    state.
  // 2. The majority of flows move IdentityManager into kSignin state, but there
  //    are few legacy flows that still allow moving into kSync state (for
  //    example, restoring from backups on platforms where this functionality
  //    exists). The behavior for existing users in kSync state is unchanged (so
  //    some code paths still check kSync state to keep the behavior
  //    consistent).
  // 3. None of the user-visible flows move IdentityManager into kSignin state.
  //    The behavior for existing users in kSync state is unchanged (so some
  //    code paths still check kSync state to keep the behavior consistent).
  // 4. All users are migrated to kSignin state. The migration will happen
  //    per-platform, so cross-platform code paths that check kSync should still
  //    be kept around (even though they are no longer executed on migrated
  //    platforms).
  // 5. When all platforms get to step 4, kSync will be completely removed from
  //    the codebase.
  //
  // Considering the upcoming deletion of kSync, features should not be adding
  // new dependencies on this value. See
  // docs.google.com/document/d/1waia7V_2SfK1vQpT40Gv1fKV_hqRQ1JgE86FRediGp0
  // for the current migration state on different platforms.
  //
  // This value was used by features that needed Chrome browser sync consent to
  // be enabled. Historically (before DICE and Project Butter) most operations
  // implicitly required this consent. See "primary account" in ./README.md.
  kSync
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_CONSENT_LEVEL_H_
