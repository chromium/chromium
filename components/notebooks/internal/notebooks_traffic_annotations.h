// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_TRAFFIC_ANNOTATIONS_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_TRAFFIC_ANNOTATIONS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace notebooks {

// TODO(crbug.com/531809229): Update policy list for traffic annotations.
net::NetworkTrafficAnnotationTag GetCreateNotebookTrafficAnnotation();
net::NetworkTrafficAnnotationTag GetCreateNotebookSourceTrafficAnnotation();
net::NetworkTrafficAnnotationTag GetListNotebooksForUserTrafficAnnotation();

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_TRAFFIC_ANNOTATIONS_H_
