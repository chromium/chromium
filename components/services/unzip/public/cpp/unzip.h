// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
#define COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace base {
class FilePath;
}

namespace unzip {

// Unzips |zip_file| into |output_dir|.
using UnzipCallback = base::OnceCallback<void(bool result)>;
void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           UnzipCallback result_callback);

// Similar to |Unzip| but only unzips files that |filter_callback| vetted.
// Note that |filter_callback| may be invoked from a background thread.
using UnzipFilterCallback =
    base::RepeatingCallback<bool(const base::FilePath& path)>;
void UnzipWithFilter(mojo::PendingRemote<mojom::Unzipper> unzipper,
                     const base::FilePath& zip_file,
                     const base::FilePath& output_dir,
                     UnzipFilterCallback filter_callback,
                     UnzipCallback result_callback);

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
