// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_SERVICE_H_

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace password_manager {

// Launches a new instance of the CSVPasswordParser service in an
// isolated, sandboxed process, and returns a remote interface to control the
// service. The lifetime of the process is tied to that of the Remote. May be
// called from any thread.
mojo::Remote<password_manager::mojom::CSVPasswordParser>
LaunchCSVPasswordParser();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_SERVICE_H_
