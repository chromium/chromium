// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_

namespace content_creation {

// All supported reaction types. Corresponding |@IntDef| definitions are
// generated on the Java side at build time.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.content_creation.reactions)
enum class ReactionType {
  CLAP = 0,
  EMOTIONAL = 1,
  EYES = 2,
  FIRE = 3,
  GRIN = 4,
  SURPRISE = 5,
  THANKS = 6,
  UNSURE = 7,

  MAX_VALUE = UNSURE
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTION_TYPES_H_