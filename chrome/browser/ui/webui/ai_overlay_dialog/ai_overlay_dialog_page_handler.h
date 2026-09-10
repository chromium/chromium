// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ttc/ttc_mes_client.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom/dom_node_id.mojom.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}

namespace ttc {

class AiOverlayDialogUntrustedUI;

class AiOverlayDialogPageHandler : public ai_overlay_dialog::mojom::PageHandler,
                                   public AiOverlayDialogController::Observer,
                                   public TtcMesClient::Observer {
 public:
  AiOverlayDialogPageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
      mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
      BrowserWindowInterface* browser,
      AiOverlayDialogUntrustedUI* untrusted_ui = nullptr);
  ~AiOverlayDialogPageHandler() override;

  // overlay_dialog::mojom::PageHandler interface
  void GetMockAudioData(GetMockAudioDataCallback callback) override;
  void UpdateAudioEnergy(float energy) override;
  void Close() override;
  void GetCursorPosition(GetCursorPositionCallback callback) override;
  void CaptureRawViewportRegion(
      int32_t x,
      int32_t y,
      int32_t width,
      int32_t height,
      CaptureRawViewportRegionCallback callback) override;
  void SetRememberedNote(ai_overlay_dialog::mojom::RememberedNotePtr note,
                         SetRememberedNoteCallback callback) override;
  void GetRememberedNotes(GetRememberedNotesCallback callback) override;
  void SaveDebugFile(ai_overlay_dialog::mojom::DebugFileType type,
                     const std::string& content) override;
  void GetImageBytes(const blink::DOMNodeIdType& dom_node_id,
                     GetImageBytesCallback callback) override;

  // Streaming methods:
  void StartStreamingSession() override;
  void SendAudioChunk(mojo_base::BigBuffer pcm_data) override;
  void SendTextInput(const std::string& text) override;
  void ReportPlaybackStatus(int64_t last_played_sequence_number) override;
  void StopStreamingSession() override;

  // TtcMesClient::Observer
  void OnStreamingStateChanged(bool connected,
                               const std::string& session_id,
                               const std::string& error_message) override;
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override;
  void OnAudioOutput(const std::vector<uint8_t>& audio_data,
                     int64_t sequence_number) override;
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override;

  void DidChangePage(const GURL& url,
                     const std::optional<std::u16string>& title,
                     const std::optional<std::string>& content);
  void UpdateCurrentPageContext(
      const std::u16string& title,
      ai_overlay_dialog::mojom::PageContentNodePtr root_node = nullptr);

  // AiOverlayDialogController::Observer
  void OnInputCaptionsVisibleChanged(bool visible) override;
  void OnOutputCaptionsVisibleChanged(bool visible) override;
  void OnUsePersonaChanged(bool use_persona) override;

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::PageHandler> receiver_;
  mojo::Remote<ai_overlay_dialog::mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<actions::ActionItem> overlay_action_item_ = nullptr;
  raw_ptr<AiOverlayDialogUntrustedUI> untrusted_ui_ = nullptr;
  std::unique_ptr<TtcMesClient> ttc_mes_client_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
