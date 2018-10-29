# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This signs the main app and helper executable, and enables "rootless"
# protections. The main app does not use library validation because it has to
# load Flash player, plugins, etc.
enforcement_flags_app="restrict"

# All the helpers (crashpad, app_mode_loader, etc.), run under library
# validation as they should not run any code not signed by Google.
enforcement_flags_helpers="${enforcement_flags_app},library"

# The installer tools are signed with the kill bit as well, as they run on
# signing machines and should never be modified.
enforcement_flags_installer_tools="${enforcement_flags_helpers},kill"

# The designated requirement suffix used when signing Chrome's binaries. It
# contains the hash of the certificate used to sign Chrome. When transitioning
# signing certs, this may include the hash of both the old and new certificate.
requirement_suffix="\
and certificate leaf = H\"c9a99324ca3fcb23dbcc36bd5fd4f9753305130a\" \
"
