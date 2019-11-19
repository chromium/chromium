// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/window_hang_monitor_win.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/thread.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace browser_watcher {

namespace {

const char kChildReadPipeSwitch[] = "child_read_pipe";
const char kChildWritePipeSwitch[] = "child_write_pipe";

// Signals used for IPC between the monitor process and the monitor.
enum IPCSignal {
  IPC_SIGNAL_INVALID,
  IPC_SIGNAL_READY,
  IPC_SIGNAL_TERMINATE_PROCESS,
  IPC_SIGNAL_CREATE_MESSAGE_WINDOW,
  IPC_SIGNAL_DELETE_MESSAGE_WINDOW,
  IPC_SIGNAL_HANG_MESSAGE_WINDOW,
};

// Sends |ipc_signal| through the |write_pipe|.
bool SendPipeSignal(HANDLE write_pipe, IPCSignal ipc_signal) {
  DWORD bytes_written = 0;
  if (!WriteFile(write_pipe, &ipc_signal, sizeof(ipc_signal), &bytes_written,
                 nullptr))
    return false;

  return bytes_written == sizeof(ipc_signal);
}

// Blocks on |read_pipe| until a signal is received into |ipc_signal|.
bool WaitForPipeSignal(HANDLE read_pipe, IPCSignal* ipc_signal) {
  CHECK(ipc_signal);
  DWORD bytes_read = 0;
  if (!ReadFile(read_pipe, ipc_signal, sizeof(*ipc_signal), &bytes_read,
                nullptr))
    return false;

  return bytes_read == sizeof(*ipc_signal);
}

// Blocks on |read_pipe| until a signal is received and returns true if it
// matches |expected_ipc_signal|.
bool WaitForSpecificPipeSignal(HANDLE read_pipe,
                               IPCSignal expected_ipc_signal) {
  IPCSignal received_signal = IPC_SIGNAL_INVALID;
  return WaitForPipeSignal(read_pipe, &received_signal) &&
         received_signal == expected_ipc_signal;
}

// Appends |handle| as a command line switch.
void AppendSwitchHandle(base::CommandLine* command_line,
                        std::string switch_name,
                        HANDLE handle) {
  command_line->AppendSwitchASCII(
      switch_name, base::NumberToString(base::win::HandleToUint32(handle)));
}

// Retrieves the |handle| associated to |switch_name| from the command line.
HANDLE GetSwitchValueHandle(base::CommandLine* command_line,
                            std::string switch_name) {
  std::string switch_string = command_line->GetSwitchValueASCII(switch_name);
  unsigned int switch_uint = 0;
  if (switch_string.empty() ||
      !base::StringToUint(switch_string, &switch_uint)) {
    DLOG(ERROR) << "Missing or invalid " << switch_name << " argument.";
    return nullptr;
  }
  return reinterpret_cast<HANDLE>(switch_uint);
}

// An instance of this class lives in the monitored process and receives signals
// and executes their associated function.
class MonitoredProcessClient {
 public:
  MonitoredProcessClient()
      : message_window_thread_("Message window thread"),
        hang_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    read_pipe_.Set(GetSwitchValueHandle(command_line, kChildReadPipeSwitch));
    write_pipe_.Set(GetSwitchValueHandle(command_line, kChildWritePipeSwitch));
  }

  ~MonitoredProcessClient() {
    if (message_window_thread_.IsRunning()) {
      DeleteMessageWindow();
    }
  }

  void RunEventLoop() {
    bool running = true;
    IPCSignal ipc_signal = IPC_SIGNAL_INVALID;
    while (running) {
      CHECK(WaitForPipeSignal(read_pipe_.Get(), &ipc_signal));
      switch (ipc_signal) {
        // The parent process should never send those.
        case IPC_SIGNAL_INVALID:
        case IPC_SIGNAL_READY:
          CHECK(false);
          break;
        case IPC_SIGNAL_TERMINATE_PROCESS:
          running = false;
          break;
        case IPC_SIGNAL_CREATE_MESSAGE_WINDOW:
          CreateMessageWindow();
          break;
        case IPC_SIGNAL_DELETE_MESSAGE_WINDOW:
          DeleteMessageWindow();
          break;
        case IPC_SIGNAL_HANG_MESSAGE_WINDOW:
          HangMessageWindow();
          break;
      }
      SendSignalToParent(IPC_SIGNAL_READY);
    }
  }

  // Creates a thread then creates the message window on it.
  void CreateMessageWindow() {
    ASSERT_TRUE(message_window_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::UI, 0)));

    bool succeeded = false;
    base::WaitableEvent created(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    ASSERT_TRUE(message_window_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MonitoredProcessClient::CreateMessageWindowInWorkerThread,
            base::Unretained(this), &succeeded, &created)));
    created.Wait();
    ASSERT_TRUE(succeeded);
  }

  // Creates a thread then creates the message window on it.
  void HangMessageWindow() {
    message_window_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                  base::Unretained(&hang_event_)));
  }

  bool SendSignalToParent(IPCSignal ipc_signal) {
    return SendPipeSignal(write_pipe_.Get(), ipc_signal);
  }

 private:
  bool EmptyMessageCallback(UINT message,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT* result) {
    EXPECT_TRUE(message_window_thread_.task_runner()->BelongsToCurrentThread());
    return false;  // Pass through to DefWindowProc.
  }

  void CreateMessageWindowInWorkerThread(bool* success,
                                         base::WaitableEvent* created) {
    CHECK(created);

    // As an alternative to checking if the name of the message window is the
    // user data directory, the hang watcher verifies that the window name is an
    // existing directory. DIR_CURRENT is used to meet this constraint.
    base::FilePath existing_dir;
    CHECK(base::PathService::Get(base::DIR_CURRENT, &existing_dir));

    message_window_.reset(new base::win::MessageWindow);
    *success = message_window_->CreateNamed(
        base::Bind(&MonitoredProcessClient::EmptyMessageCallback,
                   base::Unretained(this)),
        existing_dir.value());
    created->Signal();
  }

  void DeleteMessageWindow() {
    base::WaitableEvent deleted(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    message_window_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MonitoredProcessClient::DeleteMessageWindowInWorkerThread,
            base::Unretained(this), &deleted));
    deleted.Wait();

    message_window_thread_.Stop();
  }

  void DeleteMessageWindowInWorkerThread(base::WaitableEvent* deleted) {
    CHECK(deleted);
    message_window_.reset();
    deleted->Signal();
  }

  // The thread that holds the message window.
  base::Thread message_window_thread_;
  std::unique_ptr<base::win::MessageWindow> message_window_;

  // Event used to hang the message window.
  base::WaitableEvent hang_event_;

  // Anonymous pipe handles for IPC with the parent process.
  base::win::ScopedHandle read_pipe_;
  base::win::ScopedHandle write_pipe_;

  DISALLOW_COPY_AND_ASSIGN(MonitoredProcessClient);
};

// The monitored process main function.
MULTIPROCESS_TEST_MAIN(MonitoredProcess) {
  MonitoredProcessClient monitored_process_client;
  CHECK(monitored_process_client.SendSignalToParent(IPC_SIGNAL_READY));

  monitored_process_client.RunEventLoop();

  return 0;
}

// Manages a WindowHangMonitor that lives on a background thread.
class HangMonitorThread {
 public:
  // Instantiates the background thread.
  HangMonitorThread()
      : event_(WindowHangMonitor::WINDOW_NOT_FOUND),
        event_received_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        thread_("Hang monitor thread") {}

  ~HangMonitorThread() {
    if (hang_monitor_)
      DestroyWatcher();
  }

  // Starts the background thread and the monitor to observe Chrome message
  // window for |process|. Blocks until the monitor has been initialized.
  bool Start(base::Process process) {
    if (!thread_.StartWithOptions(
            base::Thread::Options(base::MessagePumpType::UI, 0))) {
      return false;
    }

    base::WaitableEvent complete(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    if (!thread_.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(&HangMonitorThread::StartupOnThread,
                           base::Unretained(this), std::move(process),
                           base::Unretained(&complete)))) {
      return false;
    }

    complete.Wait();

    return true;
  }

  // Returns true if a window event is detected within |timeout|.
  bool TimedWaitForEvent(base::TimeDelta timeout) {
    return event_received_.TimedWait(timeout);
  }

  // Blocks indefinitely for a window event and returns it.
  WindowHangMonitor::WindowEvent WaitForEvent() {
    event_received_.Wait();
    return event_;
  }

 private:
  // Destroys the monitor and stops the background thread. Blocks until the
  // operation completes.
  void DestroyWatcher() {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HangMonitorThread::ShutdownOnThread,
                                  base::Unretained(this)));
    // This will block until the above-posted task completes.
    thread_.Stop();
  }

  // Invoked when the monitor signals an event. Unblocks a call to
  // TimedWaitForEvent or WaitForEvent.
  void EventCallback(WindowHangMonitor::WindowEvent event) {
    if (event_received_.IsSignaled())
      ADD_FAILURE() << "Multiple calls to EventCallback.";
    event_ = event;
    event_received_.Signal();
  }

  // Initializes the WindowHangMonitor to observe the Chrome message window for
  // |process|. Signals |complete| when done.
  void StartupOnThread(base::Process process, base::WaitableEvent* complete) {
    hang_monitor_.reset(new WindowHangMonitor(
        base::TimeDelta::FromMilliseconds(100),
        base::TimeDelta::FromMilliseconds(100),
        base::Bind(&HangMonitorThread::EventCallback, base::Unretained(this))));
    hang_monitor_->Initialize(std::move(process));
    complete->Signal();
  }

  // Destroys the WindowHangMonitor.
  void ShutdownOnThread() { hang_monitor_.reset(); }

  // The detected event. Invalid if |event_received_| has not been signaled.
  WindowHangMonitor::WindowEvent event_;
  // Indicates that |event_| has been assigned in response to a callback from
  // the WindowHangMonitor.
  base::WaitableEvent event_received_;
  // The WindowHangMonitor under test.
  std::unique_ptr<WindowHangMonitor> hang_monitor_;
  // The background thread.
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(HangMonitorThread);
};

class WindowHangMonitorTest : public testing::Test {
 public:
  WindowHangMonitorTest() {}

  ~WindowHangMonitorTest() override {
    // Close process if running.
    monitored_process_.Terminate(1, false);
  }

  // Starts a child process that will be monitored. Handles to anonymous pipes
  // are passed to the command line to provide a way to communicate with the
  // child process. This function blocks until IPC_SIGNAL_READY is received.
  bool StartMonitoredProcess() {
    HANDLE child_read_pipe = nullptr;
    HANDLE child_write_pipe = nullptr;
    if (!CreatePipes(&child_read_pipe, &child_write_pipe))
      return false;

    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    command_line.AppendSwitchASCII(switches::kTestChildProcess,
                                   "MonitoredProcess");

    AppendSwitchHandle(&command_line, kChildReadPipeSwitch, child_read_pipe);
    AppendSwitchHandle(&command_line, kChildWritePipeSwitch, child_write_pipe);

    base::LaunchOptions options = {};
    // TODO(brettw) bug 748258: Share only explicit handles.
    options.inherit_mode = base::LaunchOptions::Inherit::kAll;
    monitored_process_ = base::LaunchProcess(command_line, options);
    if (!monitored_process_.IsValid())
      return false;

    return WaitForSignal(IPC_SIGNAL_READY);
  }

  void StartHangMonitor() {
    monitor_thread_.Start(monitored_process_.Duplicate());
  }

  // Sends the |ipc_signal| to the child process and wait for a IPC_SIGNAL_READY
  // response.
  bool SendSignal(IPCSignal ipc_signal) {
    if (!SendPipeSignal(write_pipe_.Get(), ipc_signal))
      return false;

    return WaitForSignal(IPC_SIGNAL_READY);
  }

  // Blocks until |ipc_signal| is received from the child process.
  bool WaitForSignal(IPCSignal ipc_signal) {
    return WaitForSpecificPipeSignal(read_pipe_.Get(), ipc_signal);
  }

  HangMonitorThread& monitor_thread() { return monitor_thread_; }

 private:
  // Creates pipes for IPC with the child process.
  bool CreatePipes(HANDLE* child_read_pipe, HANDLE* child_write_pipe) {
    CHECK(child_read_pipe);
    CHECK(child_write_pipe);
    SECURITY_ATTRIBUTES security_attributes = {
        sizeof(SECURITY_ATTRIBUTES), nullptr, true /* inherit handles */};

    HANDLE parent_read_pipe = nullptr;
    if (!CreatePipe(&parent_read_pipe, child_write_pipe, &security_attributes,
                    0)) {
      return false;
    }
    read_pipe_.Set(parent_read_pipe);

    HANDLE parent_write_pipe = nullptr;
    if (!CreatePipe(child_read_pipe, &parent_write_pipe, &security_attributes,
                    0)) {
      return false;
    }
    write_pipe_.Set(parent_write_pipe);
    return true;
  }

  // The thread that monitors the child process.
  HangMonitorThread monitor_thread_;
  // The process that is monitored.
  base::Process monitored_process_;

  // Anonymous pipe handles for IPC with the monitored process.
  base::win::ScopedHandle read_pipe_;
  base::win::ScopedHandle write_pipe_;

  DISALLOW_COPY_AND_ASSIGN(WindowHangMonitorTest);
};

}  // namespace

TEST_F(WindowHangMonitorTest, WindowNotFound) {
  ASSERT_TRUE(StartMonitoredProcess());

  StartHangMonitor();

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_TERMINATE_PROCESS));

  EXPECT_EQ(WindowHangMonitor::WINDOW_NOT_FOUND,
            monitor_thread().WaitForEvent());
}

TEST_F(WindowHangMonitorTest, WindowVanished) {
  ASSERT_TRUE(StartMonitoredProcess());

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_CREATE_MESSAGE_WINDOW));

  StartHangMonitor();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(250)));

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_DELETE_MESSAGE_WINDOW));

  EXPECT_EQ(WindowHangMonitor::WINDOW_VANISHED,
            monitor_thread().WaitForEvent());

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_TERMINATE_PROCESS));
}

TEST_F(WindowHangMonitorTest, WindowHang) {
  ASSERT_TRUE(StartMonitoredProcess());

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_CREATE_MESSAGE_WINDOW));

  StartHangMonitor();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(250)));

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_HANG_MESSAGE_WINDOW));

  EXPECT_EQ(WindowHangMonitor::WINDOW_HUNG,
            monitor_thread().WaitForEvent());

  ASSERT_TRUE(SendSignal(IPC_SIGNAL_TERMINATE_PROCESS));
}

}  // namespace browser_watcher
