// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_PIPE_READER_H_
#define CHROMEOS_DBUS_COMMON_PIPE_READER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class TaskRunner;
}

namespace net {
class FileStream;
class IOBufferWithSize;
}  // namespace net

namespace chromeos {

// Simple class to encapsulate collecting data from a pipe into a
// string.  To use:
//   - Instantiate the appropriate subclass of PipeReader
//   - Call StartIO() which will create the appropriate FDs.
//   - As data is received, the PipeReader will collect this data
//     as appropriate to the subclass.
//   - When the there is no more data to read, the PipeReader calls
//     |callback|.
class COMPONENT_EXPORT(CHROMEOS_DBUS_COMMON) PipeReader {
 public:
  using CompletionCallback =
      base::OnceCallback<void(std::optional<std::string> data)>;

  explicit PipeReader(const scoped_refptr<base::TaskRunner>& task_runner);

  PipeReader(const PipeReader&) = delete;
  PipeReader& operator=(const PipeReader&) = delete;

  ~PipeReader();

  // Starts data collection.
  // Returns the write end of the pipe if stream was setup correctly.
  // On completion, |callback| will be called with the read |data| in
  // case of success, or with nullopt in case of an error.
  // To shut down the collection delete the instance.
  base::ScopedFD StartIO(CompletionCallback callback);

 private:
  // Posts a task to read the data from the pipe. Returns
  // net::FileStream::Read()'s result.
  int RequestRead();

  // Called when |io_buffer_| is filled via |data_stream_.Read()|.
  void OnRead(int byte_count);

  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  scoped_refptr<base::TaskRunner> task_runner_;

  CompletionCallback callback_;
  std::unique_ptr<net::FileStream> data_stream_;
  std::string data_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PipeReader> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_PIPE_READER_H_
