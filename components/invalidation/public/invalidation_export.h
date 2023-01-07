// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_EXPORT_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_EXPORT_H_

// The files under components/invalidation do not currently support a shared
// library build.  There's no point in attaching attributes to them.
//
// Many of the files in this directory are imports from sync/notifier, which
// did support a shared library build.  We can use this existing set of export
// declarations as a starting point when we prepare this directory for a shared
// library build.
//
// For now, we provide dummy definitions of these tags.

#define INVALIDATION_EXPORT

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_EXPORT_H_
