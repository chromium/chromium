// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_
#define COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_canvas.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/common/metafile_utils.h"
#include "printing/mojom/print.mojom-forward.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_print_client.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrintRenderFrameHelperTest DISABLED_PrintRenderFrameHelperTest
#else
#define MAYBE_PrintRenderFrameHelperTest PrintRenderFrameHelperTest
#endif  // BUILDFLAG(IS_ANDROID)

namespace blink {
class WebLocalFrame;
class WebView;
}  // namespace blink

namespace content {
class AXTreeSnapshotter;
}

namespace printing {

class MetafileSkia;
class PrepareFrameAndViewForPrint;

// Stores reference to frame using WebVew and unique name.
// Workaround to modal dialog issue on Linux. crbug.com/236147.
// TODO(dcheng): Try to get rid of this class. However, it's a bit tricky due to
// PrepareFrameAndViewForPrint sometimes pointing to a WebLocalFrame associated
// with a RenderFrame and sometimes pointing to an internally allocated
// WebLocalFrame...
class FrameReference {
 public:
  explicit FrameReference(blink::WebLocalFrame* frame);
  FrameReference();
  FrameReference(const FrameReference&) = delete;
  FrameReference& operator=(const FrameReference&) = delete;
  ~FrameReference();

  void Reset(blink::WebLocalFrame* frame);

  blink::WebLocalFrame* GetFrame();
  blink::WebView* view();

 private:
  raw_ptr<blink::WebView> view_;
  raw_ptr<blink::WebLocalFrame> frame_;
};

// Helper to ensure that quit closures for Mojo response are called.
class ClosuresForMojoResponse
    : public base::RefCounted<ClosuresForMojoResponse> {
 public:
  ClosuresForMojoResponse();
  ClosuresForMojoResponse(const ClosuresForMojoResponse&) = delete;
  ClosuresForMojoResponse& operator=(const ClosuresForMojoResponse&) = delete;

  void SetScriptedPrintPreviewQuitClosure(base::OnceClosure quit_print_preview);
  bool HasScriptedPrintPreviewQuitClosure() const;
  void RunScriptedPrintPreviewQuitClosure();

 private:
  friend class base::RefCounted<ClosuresForMojoResponse>;
  ~ClosuresForMojoResponse();

  // Stores quit closure for the runloop that is waiting for a Mojo reply.
  base::OnceClosure scripted_print_preview_quit_closure_;
};

// PrintRenderFrameHelper handles most of the printing grunt work for
// RenderView. We plan on making print asynchronous and that will require
// copying the DOM of the document and creating a new WebView with the contents.
class PrintRenderFrameHelper
    : public blink::WebPrintClient,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<PrintRenderFrameHelper>,
      public mojom::PrintRenderFrame {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the element to be printed. Returns a null WebElement if
    // a pdf plugin element can't be extracted from the frame.
    virtual blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) = 0;

    virtual bool IsPrintPreviewEnabled() = 0;

    // If false, window.print() won't do anything.
    // The default implementation returns |true|.
    virtual bool IsScriptedPrintEnabled();

    // Whether we should send extra metadata necessary to produce a tagged
    // (accessible) PDF.
    virtual bool ShouldGenerateTaggedPDF();

    // Returns true if printing is overridden and the default behavior should be
    // skipped for |frame|.
    virtual bool OverridePrint(blink::WebLocalFrame* frame) = 0;
  };

  PrintRenderFrameHelper(content::RenderFrame* render_frame,
                         std::unique_ptr<Delegate> delegate);
  PrintRenderFrameHelper(const PrintRenderFrameHelper&) = delete;
  PrintRenderFrameHelper& operator=(const PrintRenderFrameHelper&) = delete;
  ~PrintRenderFrameHelper() override;

  // Minimum valid value for scaling. Since scaling is originally an integer
  // representing a percentage, it should never be less than this if it is
  // valid.
  static constexpr double kEpsilon = 0.01f;

  void PrintNode(const blink::WebNode& node);

  const mojo::AssociatedRemote<mojom::PrintManagerHost>& GetPrintManagerHost();

  using PreviewDocumentTestCallback =
      base::OnceCallback<void(const blink::WebDocument&)>;

  void SetWebDocumentCollectionCallbackForTest(
      PreviewDocumentTestCallback callback);

 private:
  friend class PrintRenderFrameHelperPreviewTest;
  friend class PrintRenderFrameHelperTestBase;
  FRIEND_TEST_ALL_PREFIXES(PrintRenderFrameHelperPreviewTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           PrintRequestedPages);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           BlockScriptInitiatedPrintingFromPopup);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest, PrintLayoutTest);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest, PrintWithIframe);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)

  // `kInProgress` signifies that the preview document is being rendered
  // asynchronously by a PrintRenderer.
  enum class CreatePreviewDocumentResult {
    kSuccess = 0,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kInProgress = 1,
#endif
    kFail = 2,
  };

  enum class PrintingResult {
    kOk,
    kFailPrintInit,
    kFailPrint,
    kInvalidPageRange,
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    kFailPreview,
    kInvalidSettings,
#endif
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.  Updates need to be reflected in
  // enum PrintPreviewFailureType in tools/metrics/histograms/enums.xml.
  enum class PrintPreviewErrorBuckets {
    kNone = 0,  // Always first.
    // kBadSettingDeprecated = 1,
    kMetafileCopyFailed = 2,
    // kMetafileInitFailedDeprecated = 3,
    kZeroPages = 4,
    // kMacDraftMetafileInitFailedDeprecated = 5,
    // kPageRenderedWithoutMetafileDeprecated = 6,
    // kInvalidPrinterSettingsDeprecated = 7,
    // kMetafileCaptureFailedDeprecated = 8,
    kEmptyPrinterSettings = 9,
    kLastEnum  // Always last.
  };

  enum class PrintPreviewRequestType {
    kUserInitiatedEntireFrame,
    kUserInitiatedSelection,
    kUserInitiatedContextNode,
    kScripted  // triggered by window.print().
  };

  enum class PrintRequestType {
    kRegular,
    kScripted,
  };

  // Helper to make it easy to correctly call IPCReceived() and IPCProcessed().
  class ScopedIPC {
   public:
    explicit ScopedIPC(base::WeakPtr<PrintRenderFrameHelper> weak_this);
    ScopedIPC(const ScopedIPC&) = delete;
    ScopedIPC& operator=(const ScopedIPC&) = delete;
    ~ScopedIPC();

   private:
    const base::WeakPtr<PrintRenderFrameHelper> weak_this_;
  };

  // blink::WebPrintClient:
  void WillBeDestroyed() override;

  // RenderFrameObserver:
  void OnDestruct() override;
  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;
  void DidFinishLoadForPrinting() override;
  void ScriptedPrint(bool user_initiated) override;

  void BindPrintRenderFrameReceiver(
      mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame> receiver);

  // printing::mojom::PrintRenderFrame:
  void PrintRequestedPages() override;
  void PrintWithParams(mojom::PrintPagesParamsPtr params,
                       PrintWithParamsCallback callback) override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void PrintForSystemDialog() override;
  void SetPrintPreviewUI(
      mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) override;
  void InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
      bool has_selection) override;
  void PrintPreview(base::Value::Dict settings) override;
  void OnPrintPreviewDialogClosed() override;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override;
  void PrintingDone(bool success) override;
  void ConnectToPdfRenderer() override;
  void PrintNodeUnderContextMenu() override;

  // Update |ignore_css_margins_| based on settings.
  void UpdateFrameMarginsCssInfo(const base::Value::Dict& settings);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Prepare frame for creating preview document.
  void PrepareFrameForPreviewDocument();

  // Continue creating preview document.
  void OnFramePreparedForPreviewDocument();

  // Initialize the print preview document.
  CreatePreviewDocumentResult CreatePreviewDocument();

  // Renders a print preview page. `page_index` is 0-based.
  // Returns true if print preview should continue, false on failure.
  bool RenderPreviewPage(uint32_t page_index,
                         blink::WebLocalFrame* header_footer_frame);

  // Finalize the print ready preview document.
  bool FinalizePrintReadyDocument();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Called after a preview document has been created by a PrintRenderer.
  void OnPreviewDocumentCreated(
      int document_cookie,
      base::TimeTicks begin_time,
      base::ReadOnlySharedMemoryRegion preview_document_region);
#endif

  // Finish processing the preview document created by a PrintRenderer (record
  // the render time, update the PrintPreviewContext, and finalize the print
  // ready preview document).
  bool ProcessPreviewDocument(
      base::TimeTicks begin_time,
      base::ReadOnlySharedMemoryRegion preview_document_region);

  // Helper method to calculate the scale factor for fit-to-page.
  int GetFitToPageScaleFactor(const gfx::RectF& printable_area_in_points);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // Main printing code -------------------------------------------------------

  // Print with the system dialog.
  // WARNING: |this| may be gone after this method returns.
  void Print(blink::WebLocalFrame* frame,
             const blink::WebNode& node,
             PrintRequestType print_request_type);

  // Notification when printing is done - signal tear-down/free resources.
  void DidFinishPrinting(PrintingResult result);

  // Print Settings -----------------------------------------------------------

  // Initialize print page settings with default settings.
  // Used only for native printing workflow.
  bool InitPrintSettings(blink::WebLocalFrame* frame,
                         const blink::WebNode& node);

  // Calculate number of pages in source document.
  uint32_t CalculateNumberOfPages(blink::WebLocalFrame* frame,
                                  const blink::WebNode& node);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Set options for print preset from source PDF document.
  mojom::OptionsFromDocumentParamsPtr SetOptionsFromPdfDocument();

  // Update the current print settings with new |passed_job_settings|.
  // |passed_job_settings| dictionary contains print job details such as printer
  // name, number of copies, page range, etc.
  bool UpdatePrintSettings(blink::WebLocalFrame* frame,
                           const blink::WebNode& node,
                           base::Value::Dict passed_job_settings);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // Returns final print settings from the user.
  // WARNING: |this| may be gone after this method returns.
  mojom::PrintPagesParamsPtr GetPrintSettingsFromUser(
      blink::WebLocalFrame* frame,
      const blink::WebNode& node,
      uint32_t expected_pages_count,
      PrintRequestType print_request_type);

  // Page Printing / Rendering ------------------------------------------------

  void OnFramePreparedForPrintPages();
  void PrintPages();
  bool PrintPagesNative(blink::WebLocalFrame* frame,
                        uint32_t page_count,
                        const std::vector<uint32_t>& pages_to_print);
  void FinishFramePrinting();
  // Render the frame for printing.
  bool RenderPagesForPrint(blink::WebLocalFrame* frame,
                           const blink::WebNode& node);

  // Platform-specific helper function for rendering page(s) to |metafile|.
  void PrintPageInternal(const mojom::PrintParams& params,
                         uint32_t page_index,
                         uint32_t page_count,
                         blink::WebLocalFrame* frame,
                         blink::WebLocalFrame* header_footer_frame,
                         MetafileSkia* metafile);

  // Helper methods -----------------------------------------------------------

  // Increments the IPC nesting level when an IPC message is received.
  void IPCReceived();

  // Decrements the IPC nesting level once an IPC message has been processed.
  void IPCProcessed();

  // Script Initiated Printing ------------------------------------------------

  // Return true if script initiated printing is currently
  // allowed. |user_initiated| should be true when a user event triggered the
  // script, most likely by pressing a print button on the page.
  bool IsScriptInitiatedPrintAllowed(blink::WebLocalFrame* frame,
                                     bool user_initiated);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Shows scripted print preview when options from plugin are available.
  void ShowScriptedPrintPreview();

  // WARNING: |this| may be gone after this method returns when |type| is
  // PRINT_PREVIEW_SCRIPTED.
  void RequestPrintPreview(PrintPreviewRequestType type,
                           bool already_notified_frame);

  // Checks whether print preview should continue or not.
  // Returns true if canceling, false if continuing.
  bool CheckForCancel();

  // Notifies the browser a print preview page has been rendered for modifiable
  // content.
  // `page_index` is 0-based.
  // `metafile` is the rendered page and should be valid.
  // Returns true if print preview should continue, false on failure.
  bool PreviewPageRendered(uint32_t page_index,
                           std::unique_ptr<MetafileSkia> metafile);

  // Called when the connection with the |preview_ui_| goes away.
  void OnPreviewDisconnect();
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // `settings` must be valid.
  void SetPrintPagesParams(const mojom::PrintPagesParams& settings);

  // Quits active runloop waiting for Mojo reply. It's called when
  // |print_manager_host_| is disconnected before the replies.
  void QuitActiveRunLoop();

  // Quits a runloop waiting for a Mojo reply. These are called when a Mojo
  // message gets a reply.
  void QuitScriptedPrintPreviewRunLoop();

  // Resets internal state
  void Reset();

  // WebView used only to print the selection.
  std::unique_ptr<PrepareFrameAndViewForPrint> prep_frame_view_;
  bool reset_prep_frame_view_ = false;

  mojom::PrintPagesParamsPtr print_pages_params_;
  gfx::Rect printer_printable_area_;
  bool is_print_ready_metafile_sent_ = false;
  bool ignore_css_margins_ = false;

  // Let the browser process know of a printing failure. Only set to false when
  // the failure came from the browser in the first place.
  bool notify_browser_of_print_failure_ = true;

  // Used to check the prerendering status.
  const std::unique_ptr<Delegate> delegate_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Settings used by a PrintRenderer to create a preview document.
  base::Value::Dict print_renderer_job_settings_;

  // Used to render print documents from an external source (ARC, Crostini,
  // etc.).
  mojo::AssociatedRemote<mojom::PrintRenderer> print_renderer_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Used to notify the browser of preview UI actions.
  mojo::AssociatedRemote<mojom::PrintPreviewUI> preview_ui_;
#endif

  mojo::AssociatedReceiverSet<mojom::PrintRenderFrame> receivers_;

  // Keeps track of the state of print preview between messages.
  // TODO(vitalybuka): Create PrintPreviewContext when needed and delete after
  // use. Now it's interaction with various messages is confusing.
  class PrintPreviewContext {
   public:
    PrintPreviewContext();
    PrintPreviewContext(const PrintPreviewContext&) = delete;
    PrintPreviewContext& operator=(const PrintPreviewContext&) = delete;
    ~PrintPreviewContext();

    // Initializes the print preview context. Need to be called to set
    // the |web_frame| / |web_node| to generate the print preview for.
    void InitWithFrame(blink::WebLocalFrame* web_frame);
    void InitWithNode(const blink::WebNode& web_node);

    // Dispatchs onbeforeprint/onafterprint events. Use these instead of calling
    // the WebLocalFrame version on source_frame().
    void DispatchBeforePrintEvent(
        base::WeakPtr<PrintRenderFrameHelper> weak_this);
    void DispatchAfterPrintEvent();

    // Does bookkeeping at the beginning of print preview.
    void OnPrintPreview();

    // Create the print preview document. |pages| is empty to print all pages.
    bool CreatePreviewDocument(
        std::unique_ptr<PrepareFrameAndViewForPrint> prepared_frame,
        const PageRanges& pages,
        mojom::SkiaDocumentType doc_type,
        int document_cookie,
        bool require_document_metafile);

    // Called after a page gets rendered. |page_time| is how long the
    // rendering took.
    void RenderedPreviewPage(const base::TimeDelta& page_time);

    // Called after a preview document gets rendered by a PrintRenderer.
    // |document_time| is how long the rendering took.
    void RenderedPreviewDocument(const base::TimeDelta document_time);

    // Updates the print preview context when the required pages are rendered.
    void AllPagesRendered();

    // Finalizes the print ready preview document.
    void FinalizePrintReadyDocument();

    // Cleanup after print preview finishes.
    void Finished();

    // Cleanup after print preview fails.
    void Failed(bool report_error);

    // Helper functions
    uint32_t GetNextPageIndex();
    bool IsRendering() const;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    bool IsForArc() const;
#endif
    bool IsPlugin() const;
    bool IsModifiable() const;
    bool HasSelection();
    bool IsLastPageOfPrintReadyMetafile() const;
    bool IsFinalPageRendered() const;

    // Setters
#if BUILDFLAG(IS_CHROMEOS_ASH)
    void SetIsForArc(bool is_for_arc);
#endif
    void set_error(PrintPreviewErrorBuckets error);

    // Getters
    // Original frame for which preview was requested.
    blink::WebLocalFrame* source_frame();
    // Original node for which preview was requested.
    const blink::WebNode& source_node() const;

    // Frame to be use to render preview. May be the same as source_frame(), or
    // generated from it, e.g. copy of selected block.
    blink::WebLocalFrame* prepared_frame();
    // Node to be use to render preview. May be the same as source_node(), or
    // generated from it, e.g. copy of selected block.
    const blink::WebNode& prepared_node() const;

    uint32_t total_page_count() const;
    const std::vector<uint32_t>& pages_to_render() const;
    size_t pages_rendered_count() const;
    MetafileSkia* metafile();
    ContentProxySet* typeface_content_info();

   private:
    enum class State {
      kUninitialized,  // Not ready to render.
      kInitialized,    // Ready to render.
      kRendering,      // Rendering.
      kDone            // Finished rendering.
    };

    // Reset some of the internal rendering context.
    void ClearContext();

    void CalculatePluginAttributes();

    // Specifies what to render for print preview.
    FrameReference source_frame_;
    blink::WebNode source_node_;

    std::unique_ptr<PrepareFrameAndViewForPrint> prep_frame_view_;

    // The typefaces encountered in the content during document serialization.
    ContentProxySet typeface_content_info_;

    // A document metafile is needed when not using the print compositor.
    std::unique_ptr<MetafileSkia> metafile_;

    // Total page count in the renderer.
    uint32_t total_page_count_ = 0;

    // The current page to render.
    int current_page_index_ = 0;

    // List of page indices that need to be rendered.
    std::vector<uint32_t> pages_to_render_;

    // True, if the document source is a plugin.
    bool is_plugin_ = false;

    // True, if the document source is modifiable. e.g. HTML and not PDF.
    bool is_modifiable_ = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // True, if the document source is from ARC.
    bool is_for_arc_ = false;
#endif

    // Specifies the total number of pages in the print ready metafile.
    int print_ready_metafile_page_count_ = 0;

    base::TimeDelta document_render_time_;
    base::TimeTicks begin_time_;

    PrintPreviewErrorBuckets error_ = PrintPreviewErrorBuckets::kNone;

    State state_ = State::kUninitialized;
  };

  class ScriptingThrottler {
   public:
    ScriptingThrottler();
    ScriptingThrottler(const ScriptingThrottler&) = delete;
    ScriptingThrottler& operator=(const ScriptingThrottler&) = delete;

    // Returns false if script initiated printing occurs too often.
    bool IsAllowed(blink::WebLocalFrame* frame);

    // Reset the counter for script initiated printing.
    // Scripted printing will be allowed to continue.
    void Reset();

   private:
    base::Time last_print_;
    int count_ = 0;
  };

  void SetupOnStopLoadingTimeout();
  void PrintRequestedPagesInternal(bool already_notified_frame);

  ScriptingThrottler scripting_throttler_;

  bool print_in_progress_ = false;
  PrintPreviewContext print_preview_context_;
  bool is_loading_ = false;
  bool is_scripted_preview_delayed_ = false;
  int ipc_nesting_level_ = 0;
  bool render_frame_gone_ = false;
  bool delete_pending_ = false;

  // If tagged PDF exporting is enabled, we also need to capture an
  // accessibility tree and store it in the metafile. AXTreeSnapshotter should
  // stay alive through the duration of printing one document, because text
  // drawing commands are only annotated with a DOMNodeId if accessibility
  // is enabled.
  std::unique_ptr<content::AXTreeSnapshotter> snapshotter_;

  // Used for two reasons:
  // * To give the document time to finish loading any pending resources that
  //   are desired for printing.
  // * To fix a race condition where the source is a PDF and print preview
  //   hangs because RequestPrintPreview is called before DidStopLoading() is
  //   called. This is a store for the RequestPrintPreview() call and its
  //   parameters so that it can be invoked after DidStopLoading.
  base::OnceClosure on_stop_loading_closure_;

  // This is used to report PrintWithParams() call result.
  PrintWithParamsCallback print_with_params_callback_;

  // Stores the quit closures of Mojo responses.
  scoped_refptr<ClosuresForMojoResponse> closures_for_mojo_responses_;

  bool do_deferred_print_for_system_dialog_ = false;

  mojo::AssociatedRemote<mojom::PrintManagerHost> print_manager_host_;

  // Stores a test-only callback for verifying the WebDocument values of the
  // preview document.
  PreviewDocumentTestCallback preview_document_test_callback_;

  base::WeakPtrFactory<PrintRenderFrameHelper> weak_ptr_factory_{this};
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_
