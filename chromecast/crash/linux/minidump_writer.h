// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_
#define CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromecast/crash/linux/minidump_params.h"
#include "chromecast/crash/linux/synchronized_minidump_manager.h"

namespace chromecast {

class MinidumpGenerator;

struct Attachment {
  std::string file_path;
  bool is_static;
};

// Class for writing a minidump with synchronized access to the minidumps
// directory.
class MinidumpWriter : public SynchronizedMinidumpManager {
 public:
  using DumpStateCallback = base::OnceCallback<int(const std::string&)>;

  // Constructs a writer for a minidump. If |minidump_filename| is absolute, it
  // must be a path to a file in the |dump_path_| directory. Otherwise, it
  // should be a filename only, in which case, |minidump_generator| creates
  // a minidump at $HOME/minidumps/|minidump_filename|. |params| describes the
  // minidump metadata. |dump_state_cb| is Run() to generate a log dump. Please
  // see the comments on |dump_state_cb_| below for details about this
  // parameter.
  // This does not take ownership of |minidump_generator|.
  MinidumpWriter(MinidumpGenerator* minidump_generator,
                 const std::string& minidump_filename,
                 const MinidumpParams& params,
                 DumpStateCallback dump_state_cb,
                 const std::vector<Attachment>* attachments = nullptr);

  // Like the constructor above, but the default implementation of
  // |dump_state_cb_| is used inside DoWork().
  MinidumpWriter(MinidumpGenerator* minidump_generator,
                 const std::string& minidump_filename,
                 const MinidumpParams& params,
                 const std::vector<Attachment>* attachments = nullptr);

  MinidumpWriter(const MinidumpWriter&) = delete;
  MinidumpWriter& operator=(const MinidumpWriter&) = delete;

  ~MinidumpWriter() override;

  // Acquires exclusive access to the minidumps directory and generates a
  // minidump and a log to be uploaded later. Returns 0 if successful, -1
  // otherwise.
  int Write() { return AcquireLockAndDoWork() ? 0 : -1; }

 protected:
  // MinidumpManager implementation:
  bool DoWork() override;

 private:
  MinidumpGenerator* const minidump_generator_;
  base::FilePath minidump_path_;
  const MinidumpParams params_;
  const std::vector<Attachment>* attachments_;

  // This callback is Run() to dump a log to |minidump_path_|.txt.gz, taking
  // |minidump_path_| as a parameter. It returns 0 on success, and a negative
  // integer otherwise. If a callback is not passed in the constructor, the
  // default implemementaion is used.
  DumpStateCallback dump_state_cb_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_
