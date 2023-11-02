// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_SERVICE_ACCESS_TYPE_H_
#define COMPONENTS_KEYED_SERVICE_CORE_SERVICE_ACCESS_TYPE_H_

// Some KeyedServices are accessed with the following parameter. This parameter
// defines what the caller plans to do with the service.
//
// The caller is responsible for not performing any operation that would
// result in persistent implicit records while using an OffTheRecord context.
// This flag allows the context to perform an additional check.
//
// It also leaves an opportunity to perform further checks in the future. For
// example an history service that only allow some specific methods could be
// returned.
enum class ServiceAccessType {
  // The caller plans to perform a read or write that takes place as a result
  // of the user input. Use this flag when the operation can be performed while
  // incognito (for example creating a bookmark).
  //
  // Since EXPLICIT_ACCESS means "as a result of a user action", this request
  // always succeeds.
  EXPLICIT_ACCESS,

  // The caller plans to call a method that will permanently change some data
  // in the context, as part of Chrome's implicit data logging. Use this flag
  // before performing an operation which is incompatible with the incognito
  // mode.
  IMPLICIT_ACCESS
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_SERVICE_ACCESS_TYPE_H_
