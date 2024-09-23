// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/statusor.h"

#ifndef COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_
#define COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_

namespace reporting {

class Decompression : public base::RefCountedThreadSafe<Decompression> {
 public:
  // Not copyable or movable
  Decompression(const Decompression& other) = delete;
  Decompression& operator=(const Decompression& other) = delete;

  // Factory method creates |Decompression| object.
  static scoped_refptr<Decompression> Create();

  // DecompressRecord will decompress the provided |record| and respond
  // with the callback. The compression_information provided will determine
  // which compression algorithm is used. On success the returned std::string
  // sink will contain a decompressed EncryptedWrappedRecord string. The sink
  // string then can be further updated by the caller. std::string is used
  // instead of std::string_view because ownership is taken of |record| through
  // std::move(record).
  [[nodiscard]] static std::string DecompressRecord(
      std::string record,
      CompressionInformation compression_information);

 protected:
  // Constructor can only be called by |Create| factory method.
  Decompression();

  // Refcounted object must have destructor declared protected or private.
  virtual ~Decompression();

 private:
  friend base::RefCountedThreadSafe<Decompression>;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_
