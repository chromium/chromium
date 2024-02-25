// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_PAGE_HANDLER_H_

#include <memory>
#include <string>

#include "ash/sensor_info/sensor_types.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash {

class SensorPageHandlerTest;
class SensorProvider;

// SensorPageHandler would receive sensor updates from SensorProvider through
// 'OnSensorUpdated' function and in this function forward sensor updates to TS
// side. SensorPageHandler is also inheriting sensor::mojom::PageHandler, so
// also responsible of TS side to C++ side communications.
class SensorPageHandler : public SensorObserver,
                          public sensor::mojom::PageHandler {
 public:
  // Enum class for file open status.
  enum class State {
    // Default state, and when ResetFile() called.
    kStopped,
    // When StartRecordingUpdate() is called and its post_task hasn't finish.
    kOpening,
    // When we finish 'out_file_' initialization in StartRecordingUpdate()'s
    // post_task.
    kOpened,
  };

  SensorPageHandler(Profile* profile,
                    SensorProvider* provider,
                    mojo::PendingReceiver<sensor::mojom::PageHandler> receiver,
                    base::FilePath out_file_path);

  SensorPageHandler(Profile* profile,
                    SensorProvider* provider,
                    mojo::PendingReceiver<sensor::mojom::PageHandler> receiver);

  SensorPageHandler(const SensorPageHandler&) = delete;
  SensorPageHandler& operator=(const SensorPageHandler&) = delete;
  ~SensorPageHandler() override;

  // Generates sting that should be stored. The string will look like:
  // "2023-06-13 04:08:58.424 UTC
  // 119.00
  // -0.23 0.28 9.88
  // -0.54 8.84 4.34
  // -0.00 0.00 -0.00
  // None."
  static std::string GenerateString(const ash::SensorUpdate& update,
                                    base::Time time);

  // sensor::mojom::PageHandler:
  // StartRecordingUpdate() and StopRecordingUpdate() will only change
  // 'state_' and 'save_update_'. We will write to file if 'state_' is kOpened
  // and 'save_update_' is true.
  // First few frames may not be recorded when StartRecordingUpdate() is called.
  // Because we need its post_task to finish.
  void StartRecordingUpdate() override;
  void StopRecordingUpdate() override;

  // SensorObserver:
  void OnSensorUpdated(const SensorUpdate& update) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SensorPageHandlerTest, GenerateString);
  FRIEND_TEST_ALL_PREFIXES(SensorPageHandlerTest, StartRecordingUpdate);
  FRIEND_TEST_ALL_PREFIXES(SensorPageHandlerTest, StopRecordingUpdate);
  FRIEND_TEST_ALL_PREFIXES(SensorPageHandlerTest, StartThenStop);
  FRIEND_TEST_ALL_PREFIXES(SensorPageHandlerTest, StartThenStopThenStart);

  // Returns the Download Directory path.
  static base::FilePath GetDownloadsDirectory(Profile* profile);

  // Closes file and resets 'out_file_'.
  void ResetFile();

  // Called from 'StartRecordingUpdate()'.
  void OnFileOpened(std::unique_ptr<base::File> opened_file);

  // Returns true if 'out_file_' is valid, false if otherwise.
  bool CheckOutFileForTesting();

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<Profile> profile_;
  raw_ptr<SensorProvider> provider_;
  std::unique_ptr<base::File> out_file_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Eventual state of whether we save updates to file.
  bool save_update_ = false;
  // Current File open status.
  State state_ = State::kStopped;
  mojo::Receiver<sensor::mojom::PageHandler> receiver_;
  const base::FilePath out_file_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  base::WeakPtrFactory<SensorPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_PAGE_HANDLER_H_
