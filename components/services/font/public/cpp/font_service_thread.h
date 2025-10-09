// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_SERVICE_THREAD_H_
#define COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_SERVICE_THREAD_H_

#include <stdint.h>

#include <set>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace font_service {
namespace internal {

class MappedFontFile;

// The thread which services font requests.
//
// The SkFontConfigInterface is a global singleton which can be accessed from
// multiple threads. However, mojo pipes are bound to a single thread. Because
// of this mismatch, we create a thread which owns the mojo pipe, sends and
// receives messages. The multiple threads which call through FontLoader class
// do blocking message calls to this thread.
// TODO(crbug.com/40615872): Rename FontServiceThread since it's no longer a
// thread.
class FontServiceThread : public base::RefCountedThreadSafe<FontServiceThread> {
 public:
  FontServiceThread();

  FontServiceThread(const FontServiceThread&) = delete;
  FontServiceThread& operator=(const FontServiceThread&) = delete;

  // Initializes the thread, binding to |pending_font_service| in the
  // background sequence.
  void Init(mojo::PendingRemote<mojom::FontService> pending_font_service);

  // These methods are proxies which run on your thread, post a blocking task
  // to the FontServiceThread, and wait on an event signaled from the callback.
  bool MatchFamilyName(const char family_name[],
                       SkFontStyle requested_style,
                       SkFontConfigInterface::FontIdentity* out_font_identity,
                       SkString* out_family_name,
                       SkFontStyle* out_style);
  scoped_refptr<MappedFontFile> OpenStream(
      const SkFontConfigInterface::FontIdentity& identity);

  bool FallbackFontForCharacter(
      uint32_t character,
      std::string locale,
      font_service::mojom::FontIdentityPtr* out_font_identity,
      std::string* out_family_name,
      bool* out_is_bold,
      bool* out_is_italic);
  bool FontRenderStyleForStrike(
      std::string family,
      uint32_t size,
      bool is_italic,
      bool is_bold,
      float device_scale_factor,
      font_service::mojom::FontRenderStylePtr* out_font_render_style);
  bool MatchFontByPostscriptNameOrFullFontName(
      std::string postscript_name_or_full_font_name,
      mojom::FontIdentityPtr* out_identity);

#if BUILDFLAG(ENABLE_PDF)
  void MatchFontWithFallback(std::string family,
                             bool is_bold,
                             bool is_italic,
                             uint32_t charset,
                             uint32_t fallbackFamilyType,
                             base::File* out_font_file_handle);
#endif  // BUILDFLAG(ENABLE_PDF)

 private:
  friend class base::RefCountedThreadSafe<FontServiceThread>;
  virtual ~FontServiceThread();

  void InitImpl(mojo::PendingRemote<mojom::FontService> pending_font_service);

  // Methods which run on the FontServiceThread. The public MatchFamilyName
  // calls this method, this method calls the mojo interface, and sets up the
  // callback to OnMatchFamilyNameComplete.
  void MatchFamilyNameImpl(
      base::WaitableEvent* done_event,
      const char family_name[],
      SkFontStyle requested_style,
      bool* out_valid,
      SkFontConfigInterface::FontIdentity* out_font_identity,
      SkString* out_family_name,
      SkFontStyle* out_style);

  // Called on the FontServiceThread in response to receiving a message from
  // our MatchFamily mojo IPC. This writes the data returned by mojo, and then
  // signals |done_event| to wake up the other thread.
  void OnMatchFamilyNameComplete(
      base::WaitableEvent* done_event,
      bool* out_valid,
      SkFontConfigInterface::FontIdentity* out_font_identity,
      SkString* out_family_name,
      SkFontStyle* out_style,
      mojom::FontIdentityPtr font_identity,
      const std::string& family_name,
      mojom::TypefaceStylePtr style);

  // Implementation of OpenStream; same threading restrictions as MatchFamily.
  void OpenStreamImpl(base::WaitableEvent* done_event,
                      base::File* output_file,
                      const uint32_t id_number);
  void OnOpenStreamComplete(base::WaitableEvent* done_event,
                            base::File* output_file,
                            base::File file);

  void FallbackFontForCharacterImpl(
      base::WaitableEvent* done_event,
      uint32_t character,
      std::string locale,
      bool* out_is_valid,
      font_service::mojom::FontIdentityPtr* out_font_identity,
      std::string* out_family_name,
      bool* out_is_bold,
      bool* out_is_italic);
  void OnFallbackFontForCharacterComplete(
      base::WaitableEvent* done_event,
      bool* out_valid,
      font_service::mojom::FontIdentityPtr* out_font_identity,
      std::string* out_family_name,
      bool* out_is_bold,
      bool* out_is_italic,
      mojom::FontIdentityPtr font_identity,
      const std::string& family_name,
      bool is_bold,
      bool is_italic);

  void FontRenderStyleForStrikeImpl(
      base::WaitableEvent* done_event,
      std::string family,
      uint32_t size,
      bool is_italic,
      bool is_bold,
      float device_scale_factor,
      bool* out_valid,
      mojom::FontRenderStylePtr* out_font_render_style);
  void OnFontRenderStyleForStrikeComplete(
      base::WaitableEvent* done_event,
      bool* out_valid,
      mojom::FontRenderStylePtr* out_font_render_style,
      mojom::FontRenderStylePtr font_render_style);

  void MatchFontByPostscriptNameOrFullFontNameImpl(
      base::WaitableEvent* done_event,
      bool* out_valid,
      std::string postscript_name_or_full_font_name,
      mojom::FontIdentityPtr* out_font_identity);
  void OnMatchFontByPostscriptNameOrFullFontNameComplete(
      base::WaitableEvent* done_event,
      bool* out_valid,
      mojom::FontIdentityPtr* out_font_identity,
      mojom::FontIdentityPtr font_identity);

#if BUILDFLAG(ENABLE_PDF)
  void MatchFontWithFallbackImpl(base::WaitableEvent* done_event,
                                 std::string family,
                                 bool is_bold,
                                 bool is_italic,
                                 uint32_t charset,
                                 uint32_t fallbackFamilyType,
                                 base::File* out_font_file_handle);
  void OnMatchFontWithFallbackComplete(base::WaitableEvent* done_event,
                                       base::File* out_font_file_handle,
                                       base::File file);
#endif  // BUILDFLAG(ENABLE_PDF)

  // Connection to |font_service_| has gone away. Called on the background
  // thread.
  void OnFontServiceDisconnected();

  // This member is set in InitImpl(), binding to the provided PendingRemote on
  // the background sequence.
  mojo::Remote<mojom::FontService> font_service_;

  // All WaitableEvents supplied to OpenStreamImpl() and the other *Impl()
  // functions are added here while waiting on the response from the
  // |font_service_| (FontService::OpenStream() or other such functions were
  // called, but the callbacks have not been processed yet). If |font_service_|
  // gets an error during this time all events in |pending_waitable_events_| are
  // signaled. This is necessary as when the pipe is closed the callbacks are
  // never received.
  std::set<raw_ptr<base::WaitableEvent, SetExperimental>>
      pending_waitable_events_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace internal
}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_SERVICE_THREAD_H_
