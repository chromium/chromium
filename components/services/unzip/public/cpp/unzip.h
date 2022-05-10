// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
#define COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_

#include "base/callback_forward.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/ced/src/util/encodings/encodings.h"

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

void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           mojom::UnzipOptionsPtr options,
           UnzipCallback result_callback);

using DetectEncodingCallback = base::OnceCallback<void(Encoding)>;
void DetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                    const base::FilePath& zip_file,
                    DetectEncodingCallback result_callback);

using GetExtractedSizeCallback = base::OnceCallback<void(mojom::SizePtr)>;
void GetExtractedSize(mojo::PendingRemote<mojom::Unzipper> unzipper,
                      const base::FilePath& zip_file,
                      GetExtractedSizeCallback result_callback);

}  // namespace unzip

#endif  // COMPONENTS_SERVICES_UNZIP_PUBLIC_CPP_UNZIP_H_
