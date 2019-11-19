// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_LOG_SERVICE_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_LOG_SERVICE_H_

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/logging/proto/interface_logger.pb.h"

namespace chrome_cleaner {

class InterfaceLogService : public InterfaceMetadataObserver {
 public:
  static std::unique_ptr<InterfaceLogService> Create(
      const base::StringPiece16 file_name,
      const base::StringPiece16 build_version);

  ~InterfaceLogService() override;

  // InterfaceMetadataObserver

  void ObserveCall(const LogInformation& log_information,
                   const std::map<std::string, std::string>& params) override;
  void ObserveCall(const LogInformation& log_information) override;

  // Exposes the underlying call_record_, this is for testing purposes and
  // to provide a way to print or log the recorded calls.
  std::vector<APICall> GetCallHistory() const;

  // Returns the build version of all the logged function calls.
  std::string GetBuildVersion() const;

  base::FilePath GetLogFilePath() const;

 private:
  InterfaceLogService(const base::StringPiece16 file_name,
                      const base::StringPiece16 build_version,
                      std::ofstream csv_stream);

  // TODO(joenotcharles): Currently the CallHistory is only used in the unit
  // test. Decide whether it's worth keeping.
  CallHistory call_record_;

  const base::string16 log_file_name_;
  // Stream to output CSV records to.
  std::ofstream csv_stream_;

  // Time at the creation of the object
  base::TimeTicks ticks_at_creation_{base::TimeTicks::Now()};

  // Locks access to |csv_stream_| and |call_record_|.
  mutable base::Lock lock_;

  base::TimeDelta GetTicksSinceCreation() const;

  DISALLOW_COPY_AND_ASSIGN(InterfaceLogService);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_INTERFACE_LOG_SERVICE_H_
