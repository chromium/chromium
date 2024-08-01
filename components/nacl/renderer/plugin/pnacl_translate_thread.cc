// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/nacl/renderer/plugin/pnacl_translate_thread.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <sstream>

#include "base/check.h"
#include "base/time/time.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/var.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace plugin {
namespace {

template <typename Val>
std::string MakeCommandLineArg(const char* key, const Val val) {
  std::stringstream ss;
  ss << key << val;
  return ss.str();
}

void GetLlcCommandLine(std::vector<std::string>* args,
                       size_t obj_files_size,
                       int32_t opt_level,
                       bool is_debug,
                       const std::string& architecture_attributes) {
  // TODO(dschuff): This CL override is ugly. Change llc to default to
  // using the number of modules specified in the first param, and
  // ignore multiple uses of -split-module
  args->push_back(MakeCommandLineArg("-split-module=", obj_files_size));
  args->push_back(MakeCommandLineArg("-O", opt_level));
  if (is_debug)
    args->push_back("-bitcode-format=llvm");
  if (!architecture_attributes.empty())
    args->push_back("-mattr=" + architecture_attributes);
}

void GetSubzeroCommandLine(std::vector<std::string>* args,
                           int32_t opt_level,
                           bool is_debug,
                           const std::string& architecture_attributes) {
  args->push_back(MakeCommandLineArg("-O", opt_level));
  DCHECK(!is_debug);
  // TODO(stichnot): enable this once the mattr flag formatting is
  // compatible: https://code.google.com/p/nativeclient/issues/detail?id=4132
  // if (!architecture_attributes.empty())
  //   args->push_back("-mattr=" + architecture_attributes);
}

}  // namespace

PnaclTranslateThread::PnaclTranslateThread()
    : compiler_subprocess_(nullptr),
      ld_subprocess_(nullptr),
      compiler_subprocess_active_(false),
      ld_subprocess_active_(false),
      buffer_cond_(&cond_mu_),
      done_(false),
      compile_time_(0),
      obj_files_(nullptr),
      num_threads_(0),
      nexe_file_(nullptr),
      coordinator_error_info_(nullptr),
      coordinator_(nullptr) {}

void PnaclTranslateThread::SetupState(
    const pp::CompletionCallback& finish_callback,
    NaClSubprocess* compiler_subprocess,
    NaClSubprocess* ld_subprocess,
    std::vector<base::File>* obj_files,
    int num_threads,
    base::File* nexe_file,
    ErrorInfo* error_info,
    PP_PNaClOptions* pnacl_options,
    const std::string& architecture_attributes,
    PnaclCoordinator* coordinator) {
  compiler_subprocess_ = compiler_subprocess;
  ld_subprocess_ = ld_subprocess;
  obj_files_ = obj_files;
  num_threads_ = num_threads;
  nexe_file_ = nexe_file;
  coordinator_error_info_ = error_info;
  pnacl_options_ = pnacl_options;
  architecture_attributes_ = architecture_attributes;
  coordinator_ = coordinator;

  report_translate_finished_ = finish_callback;
}

void PnaclTranslateThread::RunCompile(
    const pp::CompletionCallback& compile_finished_callback) {
  DCHECK(started());
  DCHECK(compiler_subprocess_->service_runtime());
  compiler_subprocess_active_ = true;

  // Take ownership of this IPC channel to make sure that it does not get
  // freed on the child thread when the child thread calls Shutdown().
  compiler_channel_ =
      compiler_subprocess_->service_runtime()->TakeTranslatorChannel();
  // compiler_channel_ is a IPC::SyncChannel, which is not thread-safe and
  // cannot be used directly by the child thread, so create a
  // SyncMessageFilter which can be used by the child thread.
  compiler_channel_filter_ = compiler_channel_->CreateSyncMessageFilter();

  compile_finished_callback_ = compile_finished_callback;
  translate_thread_ = std::make_unique<CompileThread>(this);
  translate_thread_->Start();
}

void PnaclTranslateThread::RunLink() {
  DCHECK(started());
  DCHECK(ld_subprocess_->service_runtime());
  ld_subprocess_active_ = true;

  // Take ownership of this IPC channel to make sure that it does not get
  // freed on the child thread when the child thread calls Shutdown().
  ld_channel_ = ld_subprocess_->service_runtime()->TakeTranslatorChannel();
  // ld_channel_ is a IPC::SyncChannel, which is not thread-safe and cannot be
  // used directly by the child thread, so create a SyncMessageFilter which
  // can be used by the child thread.
  ld_channel_filter_ = ld_channel_->CreateSyncMessageFilter();

  // Tear down the previous thread.
  translate_thread_->Join();
  translate_thread_ = std::make_unique<LinkThread>(this);
  translate_thread_->Start();
}

// Called from main thread to send bytes to the translator.
void PnaclTranslateThread::PutBytes(const void* bytes, int32_t count) {
  CHECK(bytes);
  base::AutoLock lock(cond_mu_);
  data_buffers_.push_back(std::string());
  data_buffers_.back().insert(data_buffers_.back().end(),
                              static_cast<const char*>(bytes),
                              static_cast<const char*>(bytes) + count);
  buffer_cond_.Signal();
}

void PnaclTranslateThread::EndStream() {
  base::AutoLock lock(cond_mu_);
  done_ = true;
  buffer_cond_.Signal();
}

ppapi::proxy::SerializedHandle PnaclTranslateThread::GetHandleForSubprocess(
    base::File* file,
    int32_t open_flags) {
  DCHECK(file->IsValid());
  IPC::PlatformFileForTransit file_for_transit =
      IPC::GetPlatformFileForTransit(file->GetPlatformFile(), false);

  // Using 0 disables any use of quota enforcement for this file handle.
  PP_Resource file_io = 0;

  ppapi::proxy::SerializedHandle handle;
  handle.set_file_handle(file_for_transit, open_flags, file_io);
  return handle;
}

void PnaclTranslateThread::CompileThread::Run() {
  pnacl_translate_thread_->DoCompile();
}

void PnaclTranslateThread::DoCompile() {
  {
    base::AutoLock lock(subprocess_mu_);
    // If the main thread asked us to exit in between starting the thread
    // and now, just leave now.
    if (!compiler_subprocess_active_)
      return;
  }

  std::vector<ppapi::proxy::SerializedHandle> compiler_output_files;
  for (base::File& obj_file : *obj_files_) {
    compiler_output_files.push_back(
        GetHandleForSubprocess(&obj_file, PP_FILEOPENFLAG_WRITE));
  }

  pp::Core* core = pp::Module::Get()->core();
  base::TimeTicks do_compile_start_time = base::TimeTicks::Now();

  std::vector<std::string> args;
  if (pnacl_options_->use_subzero) {
    GetSubzeroCommandLine(&args, pnacl_options_->opt_level,
                          PP_ToBool(pnacl_options_->is_debug),
                          architecture_attributes_);
  } else {
    GetLlcCommandLine(&args, obj_files_->size(),
                      pnacl_options_->opt_level,
                      PP_ToBool(pnacl_options_->is_debug),
                      architecture_attributes_);
  }

  bool success = false;
  std::string error_str;
  if (!compiler_channel_filter_->Send(
      new PpapiMsg_PnaclTranslatorCompileInit(
          num_threads_, compiler_output_files, args, &success, &error_str))) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                    "Compile stream init failed: "
                    "reply not received from PNaCl translator "
                    "(it probably crashed)");
    return;
  }
  if (!success) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                    std::string("Stream init failed: ") + error_str);
    return;
  }

  // llc process is started.
  while(!done_ || data_buffers_.size() > 0) {
    cond_mu_.Acquire();
    while(!done_ && data_buffers_.size() == 0) {
      buffer_cond_.Wait();
    }
    if (data_buffers_.size() > 0) {
      std::string data;
      data.swap(data_buffers_.front());
      data_buffers_.pop_front();
      cond_mu_.Release();

      if (!compiler_channel_filter_->Send(
              new PpapiMsg_PnaclTranslatorCompileChunk(data, &success))) {
        TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                        "Compile stream chunk failed: "
                        "reply not received from PNaCl translator "
                        "(it probably crashed)");
        return;
      }
      if (!success) {
        // If the error was reported by the translator, then we fall through
        // and call PpapiMsg_PnaclTranslatorCompileEnd, which returns a string
        // describing the error, which we can then send to the Javascript
        // console.
        break;
      }
      core->CallOnMainThread(
          0,
          coordinator_->GetCompileProgressCallback(data.size()),
          PP_OK);
    } else {
      cond_mu_.Release();
    }
  }
  // Finish llc.
  if (!compiler_channel_filter_->Send(
          new PpapiMsg_PnaclTranslatorCompileEnd(&success, &error_str))) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                    "Compile stream end failed: "
                    "reply not received from PNaCl translator "
                    "(it probably crashed)");
    return;
  }
  if (!success) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL, error_str);
    return;
  }
  compile_time_ =
    (base::TimeTicks::Now() - do_compile_start_time).InMicroseconds();
  nacl::PPBNaClPrivate::LogTranslateTime("NaCl.Perf.PNaClLoadTime.CompileTime",
                                         compile_time_);
  nacl::PPBNaClPrivate::LogTranslateTime(
      pnacl_options_->use_subzero
          ? "NaCl.Perf.PNaClLoadTime.CompileTime.Subzero"
          : "NaCl.Perf.PNaClLoadTime.CompileTime.LLC",
      compile_time_);

  // Shut down the compiler subprocess.
  {
    base::AutoLock lock(subprocess_mu_);
    compiler_subprocess_active_ = false;
  }

  core->CallOnMainThread(0, compile_finished_callback_, PP_OK);
}

void PnaclTranslateThread::LinkThread::Run() {
  pnacl_translate_thread_->DoLink();
}

void PnaclTranslateThread::DoLink() {
  {
    base::AutoLock lock(subprocess_mu_);
    // If the main thread asked us to exit in between starting the thread
    // and now, just leave now.
    if (!ld_subprocess_active_)
      return;
  }

  // Reset object files for reading first.  We do this before duplicating
  // handles/FDs to prevent any handle/FD leaks in case any of the Seek()
  // calls fail.
  for (base::File& obj_file : *obj_files_) {
    if (obj_file.Seek(base::File::FROM_BEGIN, 0) != 0) {
      TranslateFailed(PP_NACL_ERROR_PNACL_LD_SETUP,
                      "Link process could not reset object file");
      return;
    }
  }

  ppapi::proxy::SerializedHandle nexe_file =
      GetHandleForSubprocess(nexe_file_, PP_FILEOPENFLAG_WRITE);
  std::vector<ppapi::proxy::SerializedHandle> ld_input_files;
  for (base::File& obj_file : *obj_files_) {
    ld_input_files.push_back(
        GetHandleForSubprocess(&obj_file, PP_FILEOPENFLAG_READ));
  }

  base::TimeTicks link_start_time = base::TimeTicks::Now();
  bool success = false;
  bool sent = ld_channel_filter_->Send(
      new PpapiMsg_PnaclTranslatorLink(ld_input_files, nexe_file, &success));
  if (!sent) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LD_INTERNAL,
                    "link failed: reply not received from linker.");
    return;
  }
  if (!success) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LD_INTERNAL,
                    "link failed: linker returned failure status.");
    return;
  }

  nacl::PPBNaClPrivate::LogTranslateTime(
      "NaCl.Perf.PNaClLoadTime.LinkTime",
      (base::TimeTicks::Now() - link_start_time).InMicroseconds());

  // Shut down the ld subprocess.
  {
    base::AutoLock lock(subprocess_mu_);
    ld_subprocess_active_ = false;
  }

  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, report_translate_finished_, PP_OK);
}

void PnaclTranslateThread::TranslateFailed(
    PP_NaClError err_code,
    const std::string& error_string) {
  pp::Core* core = pp::Module::Get()->core();
  if (coordinator_error_info_->message().empty()) {
    // Only use our message if one hasn't already been set by the coordinator
    // (e.g. pexe load failed).
    coordinator_error_info_->SetReport(err_code,
                                       std::string("PnaclCoordinator: ") +
                                       error_string);
  }
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
}

void PnaclTranslateThread::AbortSubprocesses() {
  {
    base::AutoLock lock(subprocess_mu_);
    if (compiler_subprocess_ && compiler_subprocess_active_) {
      // We only run the service_runtime's Shutdown and do not run the
      // NaClSubprocess Shutdown, which would otherwise nullify some
      // pointers that could still be in use (srpc_client, etc.).
      compiler_subprocess_->service_runtime()->Shutdown();
      compiler_subprocess_active_ = false;
    }
    if (ld_subprocess_ && ld_subprocess_active_) {
      ld_subprocess_->service_runtime()->Shutdown();
      ld_subprocess_active_ = false;
    }
  }
  base::AutoLock lock(cond_mu_);
  done_ = true;
  // Free all buffered bitcode chunks
  data_buffers_.clear();
  buffer_cond_.Signal();
}

PnaclTranslateThread::~PnaclTranslateThread() {
  AbortSubprocesses();
  if (translate_thread_)
    translate_thread_->Join();
}

} // namespace plugin
