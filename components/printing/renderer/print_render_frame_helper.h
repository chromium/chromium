// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_
#define COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "printing/buildflags/buildflags.h"
#include "printing/common/metafile_utils.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "ui/gfx/geometry/size.h"

struct PrintMsg_Print_Params;
struct PrintMsg_PrintPages_Params;
struct PrintMsg_PrintFrame_Params;
struct PrintHostMsg_SetOptionsFromDocument_Params;

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PrintRenderFrameHelperTest DISABLED_PrintRenderFrameHelperTest
#define MAYBE_PrintRenderFrameHelperPreviewTest \
  DISABLED_PrintRenderFrameHelperPreviewTest
#else
#define MAYBE_PrintRenderFrameHelperTest PrintRenderFrameHelperTest
#define MAYBE_PrintRenderFrameHelperPreviewTest \
  PrintRenderFrameHelperPreviewTest
#endif  // defined(OS_ANDROID)

namespace base {
class DictionaryValue;
}

namespace blink {
class WebLocalFrame;
class WebView;
}

namespace printing {

struct PageSizeMargins;
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
  ~FrameReference();

  void Reset(blink::WebLocalFrame* frame);

  blink::WebLocalFrame* GetFrame();
  blink::WebView* view();

 private:
  blink::WebView* view_;
  blink::WebLocalFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(FrameReference);
};

// PrintRenderFrameHelper handles most of the printing grunt work for
// RenderView. We plan on making print asynchronous and that will require
// copying the DOM of the document and creating a new WebView with the contents.
class PrintRenderFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<PrintRenderFrameHelper>,
      public mojom::PrintRenderFrame {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Cancels prerender if it's currently in progress and returns true if the
    // cancellation succeeded.
    virtual bool CancelPrerender(content::RenderFrame* render_frame) = 0;

    // Returns the element to be printed. Returns a null WebElement if
    // a pdf plugin element can't be extracted from the frame.
    virtual blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) = 0;

    virtual bool IsPrintPreviewEnabled() = 0;

    // If false, window.print() won't do anything.
    // The default implementation returns |true|.
    virtual bool IsScriptedPrintEnabled();

    // Returns true if printing is overridden and the default behavior should be
    // skipped for |frame|.
    virtual bool OverridePrint(blink::WebLocalFrame* frame) = 0;
  };

  PrintRenderFrameHelper(content::RenderFrame* render_frame,
                         std::unique_ptr<Delegate> delegate);
  ~PrintRenderFrameHelper() override;

  // Minimum valid value for scaling. Since scaling is originally an integer
  // representing a percentage, it should never be less than this if it is
  // valid.
  static constexpr double kEpsilon = 0.01f;

  // Disable print preview and switch to system dialog printing even if full
  // printing is build-in. This method is used by CEF.
  static void DisablePreview();

  bool IsPrintingEnabled() const;

  void PrintNode(const blink::WebNode& node);

  // Get the scale factor. Returns |input_scale_factor| if it is valid and
  // |is_pdf| is false, and 1.0f otherwise.
  static double GetScaleFactor(double input_scale_factor, bool is_pdf);

 private:
  friend class PrintRenderFrameHelperTestBase;
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperPreviewTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           PrintRequestedPages);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest,
                           BlockScriptInitiatedPrintingFromPopup);
#if defined(OS_WIN) || defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest, PrintLayoutTest);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_PrintRenderFrameHelperTest, PrintWithIframe);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  // CREATE_IN_PROGRESS signifies that the preview document is being rendered
  // asynchronously by a PrintRenderer.
  enum CreatePreviewDocumentResult {
    CREATE_SUCCESS,
    CREATE_IN_PROGRESS,
    CREATE_FAIL,
  };

  enum PrintingResult {
    OK,
    FAIL_PRINT_INIT,
    FAIL_PRINT,
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    FAIL_PREVIEW,
    INVALID_SETTINGS,
#endif
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.  Updates need to be reflected in
  // enum PrintPreviewFailureType in tools/metrics/histograms/enums.xml.
  enum PrintPreviewErrorBuckets {
    PREVIEW_ERROR_NONE = 0,  // Always first.
    PREVIEW_ERROR_BAD_SETTING = 1,
    PREVIEW_ERROR_METAFILE_COPY_FAILED = 2,
    PREVIEW_ERROR_METAFILE_INIT_FAILED_DEPRECATED = 3,
    PREVIEW_ERROR_ZERO_PAGES = 4,
    PREVIEW_ERROR_MAC_DRAFT_METAFILE_INIT_FAILED_DEPRECATED = 5,
    PREVIEW_ERROR_PAGE_RENDERED_WITHOUT_METAFILE_DEPRECATED = 6,
    PREVIEW_ERROR_INVALID_PRINTER_SETTINGS = 7,
    PREVIEW_ERROR_METAFILE_CAPTURE_FAILED = 8,
    PREVIEW_ERROR_LAST_ENUM  // Always last.
  };

  enum PrintPreviewRequestType {
    PRINT_PREVIEW_USER_INITIATED_ENTIRE_FRAME,
    PRINT_PREVIEW_USER_INITIATED_SELECTION,
    PRINT_PREVIEW_USER_INITIATED_CONTEXT_NODE,
    PRINT_PREVIEW_SCRIPTED  // triggered by window.print().
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

  // RenderFrameObserver implementation.
  void OnDestruct() override;
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;
  void ScriptedPrint(bool user_initiated) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void BindPrintRenderFrameReceiver(
      mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame> receiver);

  // printing::mojom::PrintRenderFrame:
  void PrintRequestedPages() override;
  void PrintForSystemDialog() override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void InitiatePrintPreview(
      mojom::PrintRendererAssociatedPtrInfo print_renderer,
      bool has_selection) override;
  void OnPrintPreviewDialogClosed() override;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void PrintingDone(bool success) override;
  void SetPrintingEnabled(bool enabled) override;

  // Message handlers ---------------------------------------------------------
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void OnPrintPreview(const base::DictionaryValue& settings);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void OnPrintFrameContent(const PrintMsg_PrintFrame_Params& params);

  // Get |page_size| and |content_area| information from
  // |page_layout_in_points|.
  void GetPageSizeAndContentAreaFromPageLayout(
      const PageSizeMargins& page_layout_in_points,
      gfx::Size* page_size,
      gfx::Rect* content_area);

  // Update |ignore_css_margins_| based on settings.
  void UpdateFrameMarginsCssInfo(const base::DictionaryValue& settings);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Prepare frame for creating preview document.
  void PrepareFrameForPreviewDocument();

  // Continue creating preview document.
  void OnFramePreparedForPreviewDocument();

  // Initialize the print preview document.
  CreatePreviewDocumentResult CreatePreviewDocument();

  // Renders a print preview page. |page_number| is 0-based.
  // Returns true if print preview should continue, false on failure.
  bool RenderPreviewPage(int page_number);

  // Finalize the print ready preview document.
  bool FinalizePrintReadyDocument();

  // Called after a preview document has been created by a PrintRenderer.
  void OnPreviewDocumentCreated(
      int document_cookie,
      base::TimeTicks begin_time,
      base::ReadOnlySharedMemoryRegion preview_document_region);

  // Finish processing the preview document created by a PrintRenderer (record
  // the render time, update the PrintPreviewContext, and finalize the print
  // ready preview document).
  bool ProcessPreviewDocument(
      base::TimeTicks begin_time,
      base::ReadOnlySharedMemoryRegion preview_document_region);

  // Helper method to calculate the scale factor for fit-to-page.
  int GetFitToPageScaleFactor(const gfx::Rect& printable_area_in_points);
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
  bool InitPrintSettings(bool fit_to_paper_size);

  // Calculate number of pages in source document.
  bool CalculateNumberOfPages(blink::WebLocalFrame* frame,
                              const blink::WebNode& node,
                              int* number_of_pages);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Set options for print preset from source PDF document.
  bool SetOptionsFromPdfDocument(
      PrintHostMsg_SetOptionsFromDocument_Params* options);

  // Update the current print settings with new |passed_job_settings|.
  // |passed_job_settings| dictionary contains print job details such as printer
  // name, number of copies, page range, etc.
  bool UpdatePrintSettings(blink::WebLocalFrame* frame,
                           const blink::WebNode& node,
                           const base::DictionaryValue& passed_job_settings);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // Get final print settings from the user.
  // WARNING: |this| may be gone after this method returns.
  void GetPrintSettingsFromUser(blink::WebLocalFrame* frame,
                                const blink::WebNode& node,
                                int expected_pages_count,
                                PrintRequestType print_request_type,
                                PrintMsg_PrintPages_Params* print_settings);

  // Page Printing / Rendering ------------------------------------------------

  void OnFramePreparedForPrintPages();
  void PrintPages();
  bool PrintPagesNative(blink::WebLocalFrame* frame,
                        int page_count,
                        bool is_pdf);
  void FinishFramePrinting();
  // Render the frame for printing.
  bool RenderPagesForPrint(blink::WebLocalFrame* frame,
                           const blink::WebNode& node);

  // Platform-specific helper function for rendering page(s) to |metafile|.
  void PrintPageInternal(const PrintMsg_Print_Params& params,
                         int page_number,
                         int page_count,
                         double scale_factor,
                         blink::WebLocalFrame* frame,
                         MetafileSkia* metafile,
                         gfx::Size* page_size_in_dpi,
                         gfx::Rect* content_area_in_dpi);

  // Renders page contents from |frame| to |content_area| of |canvas|.
  // |page_number| is zero-based.
  // When method is called, canvas should be setup to draw to |canvas_area|
  // with |scale_factor|.
  static float RenderPageContent(blink::WebLocalFrame* frame,
                                 int page_number,
                                 const gfx::Rect& canvas_area,
                                 const gfx::Rect& content_area,
                                 double scale_factor,
                                 cc::PaintCanvas* canvas);

  // Helper methods -----------------------------------------------------------

  // Increments the IPC nesting level when an IPC message is received.
  void IPCReceived();

  // Decrements the IPC nesting level once an IPC message has been processed.
  void IPCProcessed();

  // Helper method to get page layout in points and fit to page if needed.
  static void ComputePageLayoutInPointsForCss(
      blink::WebLocalFrame* frame,
      int page_index,
      const PrintMsg_Print_Params& default_params,
      bool ignore_css_margins,
      double* scale_factor,
      PageSizeMargins* page_layout_in_points);

  // Return an array of pages to print given the print |params| and an expected
  // |page_count|. Page numbers are zero-based.
  static std::vector<int> GetPrintedPages(
      const PrintMsg_PrintPages_Params& params,
      int page_count);

  // Given the |device| and |canvas| to draw on, prints the appropriate headers
  // and footers using strings from |header_footer_info| on to the canvas.
  static void PrintHeaderAndFooter(cc::PaintCanvas* canvas,
                                   int page_number,
                                   int total_pages,
                                   const blink::WebLocalFrame& source_frame,
                                   float webkit_scale_factor,
                                   const PageSizeMargins& page_layout_in_points,
                                   const PrintMsg_Print_Params& params);

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
  void RequestPrintPreview(PrintPreviewRequestType type);

  // Checks whether print preview should continue or not.
  // Returns true if canceling, false if continuing.
  bool CheckForCancel();

  // Notifies the browser a print preview page has been rendered for modifiable
  // content.
  // |page_number| is 0-based.
  // |metafile| is the rendered page and should be valid.
  // Returns true if print preview should continue, false on failure.
  bool PreviewPageRendered(int page_number,
                           std::unique_ptr<MetafileSkia> metafile);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  void SetPrintPagesParams(const PrintMsg_PrintPages_Params& settings);

  // WebView used only to print the selection.
  std::unique_ptr<PrepareFrameAndViewForPrint> prep_frame_view_;
  bool reset_prep_frame_view_ = false;

  std::unique_ptr<PrintMsg_PrintPages_Params> print_pages_params_;
  gfx::Rect printer_printable_area_;
  bool is_print_ready_metafile_sent_ = false;
  bool ignore_css_margins_ = false;

  bool is_printing_enabled_ = true;

  // Let the browser process know of a printing failure. Only set to false when
  // the failure came from the browser in the first place.
  bool notify_browser_of_print_failure_ = true;

  // Used to check the prerendering status.
  const std::unique_ptr<Delegate> delegate_;

  // Settings used by a PrintRenderer to create a preview document.
  base::Value print_renderer_job_settings_;

  // Used to render print documents from an external source (ARC, Crostini,
  // etc.).
  mojom::PrintRendererAssociatedPtr print_renderer_;

  mojo::AssociatedReceiverSet<mojom::PrintRenderFrame> receivers_;

  // Keeps track of the state of print preview between messages.
  // TODO(vitalybuka): Create PrintPreviewContext when needed and delete after
  // use. Now it's interaction with various messages is confusing.
  class PrintPreviewContext {
   public:
    PrintPreviewContext();
    ~PrintPreviewContext();

    // Initializes the print preview context. Need to be called to set
    // the |web_frame| / |web_node| to generate the print preview for.
    void InitWithFrame(blink::WebLocalFrame* web_frame);
    void InitWithNode(const blink::WebNode& web_node);

    // Does bookkeeping at the beginning of print preview.
    void OnPrintPreview();

    // Create the print preview document. |pages| is empty to print all pages.
    bool CreatePreviewDocument(
        std::unique_ptr<PrepareFrameAndViewForPrint> prepared_frame,
        const std::vector<int>& pages,
        SkiaDocumentType doc_type,
        int document_cookie);

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
    int GetNextPageNumber();
    bool IsRendering() const;
    bool IsForArc() const;
    bool IsModifiable() const;
    bool IsPdf() const;
    bool HasSelection();
    bool IsLastPageOfPrintReadyMetafile() const;
    bool IsFinalPageRendered() const;

    // Setters
    void SetIsForArc(bool is_for_arc);
    void set_error(enum PrintPreviewErrorBuckets error);

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

    int total_page_count() const;
    const std::vector<int>& pages_to_render() const;
    MetafileSkia* metafile();
    int last_error() const;

   private:
    enum State {
      UNINITIALIZED,  // Not ready to render.
      INITIALIZED,    // Ready to render.
      RENDERING,      // Rendering.
      DONE            // Finished rendering.
    };

    // Reset some of the internal rendering context.
    void ClearContext();

    void CalculateIsModifiable();

    void CalculateIsPdf();

    // Specifies what to render for print preview.
    FrameReference source_frame_;
    blink::WebNode source_node_;

    std::unique_ptr<PrepareFrameAndViewForPrint> prep_frame_view_;
    std::unique_ptr<MetafileSkia> metafile_;

    // Total page count in the renderer.
    int total_page_count_ = 0;

    // The current page to render.
    int current_page_index_ = 0;

    // List of page indices that need to be rendered.
    std::vector<int> pages_to_render_;

    // True, if the document source is modifiable. e.g. HTML and not PDF.
    bool is_modifiable_ = true;

    // True, if the document source is a PDF. Used to distinguish from
    // other plugins such as Flash.
    bool is_pdf_ = false;

    // True, if the document source is from ARC.
    bool is_for_arc_ = false;

    // Specifies the total number of pages in the print ready metafile.
    int print_ready_metafile_page_count_ = 0;

    base::TimeDelta document_render_time_;
    base::TimeTicks begin_time_;

    enum PrintPreviewErrorBuckets error_ = PREVIEW_ERROR_NONE;

    State state_ = UNINITIALIZED;

    DISALLOW_COPY_AND_ASSIGN(PrintPreviewContext);
  };

  class ScriptingThrottler {
   public:
    ScriptingThrottler();

    // Returns false if script initiated printing occurs too often.
    bool IsAllowed(blink::WebLocalFrame* frame);

    // Reset the counter for script initiated printing.
    // Scripted printing will be allowed to continue.
    void Reset();

   private:
    base::Time last_print_;
    int count_ = 0;
    DISALLOW_COPY_AND_ASSIGN(ScriptingThrottler);
  };

  ScriptingThrottler scripting_throttler_;

  bool print_node_in_progress_ = false;
  PrintPreviewContext print_preview_context_;
  bool is_loading_ = false;
  bool is_scripted_preview_delayed_ = false;
  int ipc_nesting_level_ = 0;
  bool render_frame_gone_ = false;

  // Used to fix a race condition where the source is a PDF and print preview
  // hangs because RequestPrintPreview is called before DidStopLoading() is
  // called. This is a store for the RequestPrintPreview() call and its
  // parameters so that it can be invoked after DidStopLoading.
  base::OnceClosure on_stop_loading_closure_;

  base::WeakPtrFactory<PrintRenderFrameHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintRenderFrameHelper);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_RENDERER_PRINT_RENDER_FRAME_HELPER_H_
