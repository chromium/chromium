// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/print_dialog_gtk.h"

#include <gtk/gtkunixprint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/printing_gtk_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "printing/metafile.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "ui/aura/window.h"

#if defined(USE_X11)
#include "ui/events/platform/x11/x11_event_source.h"  // nogncheck
#endif

using content::BrowserThread;
using printing::PageRanges;
using printing::PrintSettings;

namespace {

#if defined(USE_CUPS)
// CUPS Duplex attribute and values.
const char kCUPSDuplex[] = "cups-Duplex";
const char kDuplexNone[] = "None";
const char kDuplexTumble[] = "DuplexTumble";
const char kDuplexNoTumble[] = "DuplexNoTumble";
#endif

constexpr int kPaperSizeTresholdMicrons = 100;
constexpr int kMicronsInMm = 1000;

// Checks whether |gtk_paper_size| can be used to represent user selected media.
// In fuzzy match mode checks that paper sizes are "close enough" (less than
// 1mm difference). In the exact mode, looks for the paper with the same PPD
// name and "close enough" size.
bool PaperSizeMatch(GtkPaperSize* gtk_paper_size,
                    const PrintSettings::RequestedMedia& media,
                    bool fuzzy_match) {
  if (!gtk_paper_size)
    return false;

  gfx::Size paper_size_microns(
      static_cast<int>(gtk_paper_size_get_width(gtk_paper_size, GTK_UNIT_MM) *
                           kMicronsInMm +
                       0.5),
      static_cast<int>(gtk_paper_size_get_height(gtk_paper_size, GTK_UNIT_MM) *
                           kMicronsInMm +
                       0.5));
  int diff = std::max(
      std::abs(paper_size_microns.width() - media.size_microns.width()),
      std::abs(paper_size_microns.height() - media.size_microns.height()));
  bool close_enough = diff <= kPaperSizeTresholdMicrons;
  if (fuzzy_match)
    return close_enough;

  return close_enough && !media.vendor_id.empty() &&
         media.vendor_id == gtk_paper_size_get_ppd_name(gtk_paper_size);
}

// Looks up a paper size matching (in terms of PaperSizeMatch) the user selected
// media in the paper size list reported by GTK. Returns nullptr if there's no
// match found.
GtkPaperSize* FindPaperSizeMatch(GList* gtk_paper_sizes,
                                 const PrintSettings::RequestedMedia& media) {
  GtkPaperSize* first_fuzzy_match = nullptr;
  for (GList* p = gtk_paper_sizes; p && p->data; p = g_list_next(p)) {
    GtkPaperSize* gtk_paper_size = static_cast<GtkPaperSize*>(p->data);
    if (PaperSizeMatch(gtk_paper_size, media, false))
      return gtk_paper_size;

    if (!first_fuzzy_match && PaperSizeMatch(gtk_paper_size, media, true))
      first_fuzzy_match = gtk_paper_size;
  }
  return first_fuzzy_match;
}

class StickyPrintSettingGtk {
 public:
  StickyPrintSettingGtk() : last_used_settings_(gtk_print_settings_new()) {}
  ~StickyPrintSettingGtk() {
    NOTREACHED();  // Intended to be used with base::NoDestructor.
  }

  GtkPrintSettings* settings() { return last_used_settings_; }

  void SetLastUsedSettings(GtkPrintSettings* settings) {
    DCHECK(last_used_settings_);
    g_object_unref(last_used_settings_);
    last_used_settings_ = gtk_print_settings_copy(settings);
  }

 private:
  GtkPrintSettings* last_used_settings_;

  DISALLOW_COPY_AND_ASSIGN(StickyPrintSettingGtk);
};

StickyPrintSettingGtk& GetLastUsedSettings() {
  static base::NoDestructor<StickyPrintSettingGtk> settings;
  return *settings;
}

// Helper class to track GTK printers.
class GtkPrinterList {
 public:
  GtkPrinterList() { gtk_enumerate_printers(SetPrinter, this, nullptr, TRUE); }

  ~GtkPrinterList() {
    for (GtkPrinter* printer : printers_)
      g_object_unref(printer);
  }

  // Can return nullptr if there's no default printer. E.g. Printer on a laptop
  // is "home_printer", but the laptop is at work.
  GtkPrinter* default_printer() { return default_printer_; }

  // Can return nullptr if the printer cannot be found due to:
  // - Printer list out of sync with printer dialog UI.
  // - Querying for non-existant printers like 'Print to PDF'.
  GtkPrinter* GetPrinterWithName(const std::string& name) {
    if (name.empty())
      return nullptr;

    for (GtkPrinter* printer : printers_) {
      if (gtk_printer_get_name(printer) == name)
        return printer;
    }

    return nullptr;
  }

 private:
  // Callback function used by gtk_enumerate_printers() to get all printer.
  static gboolean SetPrinter(GtkPrinter* printer, gpointer data) {
    GtkPrinterList* printer_list = reinterpret_cast<GtkPrinterList*>(data);
    if (gtk_printer_is_default(printer))
      printer_list->default_printer_ = printer;

    g_object_ref(printer);
    printer_list->printers_.push_back(printer);

    return FALSE;
  }

  std::vector<GtkPrinter*> printers_;
  GtkPrinter* default_printer_ = nullptr;
};

}  // namespace

// static
printing::PrintDialogGtkInterface* PrintDialogGtk::CreatePrintDialog(
    PrintingContextLinux* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return new PrintDialogGtk(context);
}

PrintDialogGtk::PrintDialogGtk(PrintingContextLinux* context)
    : context_(context) {}

PrintDialogGtk::~PrintDialogGtk() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (dialog_) {
    aura::Window* parent = libgtkui::GetAuraTransientParent(dialog_);
    if (parent) {
      parent->RemoveObserver(this);
      libgtkui::ClearAuraTransientParent(dialog_);
    }
    gtk_widget_destroy(dialog_);
    dialog_ = nullptr;
  }
  if (gtk_settings_) {
    g_object_unref(gtk_settings_);
    gtk_settings_ = nullptr;
  }
  if (page_setup_) {
    g_object_unref(page_setup_);
    page_setup_ = nullptr;
  }
  if (printer_) {
    g_object_unref(printer_);
    printer_ = nullptr;
  }
}

void PrintDialogGtk::UseDefaultSettings() {
  DCHECK(!page_setup_);
  DCHECK(!printer_);

  // |gtk_settings_| is a new copy.
  gtk_settings_ = gtk_print_settings_copy(GetLastUsedSettings().settings());
  page_setup_ = gtk_page_setup_new();

  InitPrintSettings(std::make_unique<PrintSettings>());
}

void PrintDialogGtk::UpdateSettings(
    std::unique_ptr<printing::PrintSettings> settings) {
  if (!gtk_settings_)
    gtk_settings_ = gtk_print_settings_copy(GetLastUsedSettings().settings());

  auto printer_list = std::make_unique<GtkPrinterList>();
  printer_ = printer_list->GetPrinterWithName(
      base::UTF16ToUTF8(settings->device_name()));
  if (printer_) {
    g_object_ref(printer_);
    gtk_print_settings_set_printer(gtk_settings_,
                                   gtk_printer_get_name(printer_));
    if (!page_setup_) {
      page_setup_ = gtk_printer_get_default_page_size(printer_);
    }
  }

  gtk_print_settings_set_n_copies(gtk_settings_, settings->copies());
  gtk_print_settings_set_collate(gtk_settings_, settings->collate());

#if defined(USE_CUPS)
  std::string color_value;
  std::string color_setting_name;
  printing::GetColorModelForMode(settings->color(), &color_setting_name,
                                 &color_value);
  gtk_print_settings_set(gtk_settings_, color_setting_name.c_str(),
                         color_value.c_str());

  if (settings->duplex_mode() != printing::UNKNOWN_DUPLEX_MODE) {
    const char* cups_duplex_mode = nullptr;
    switch (settings->duplex_mode()) {
      case printing::LONG_EDGE:
        cups_duplex_mode = kDuplexNoTumble;
        break;
      case printing::SHORT_EDGE:
        cups_duplex_mode = kDuplexTumble;
        break;
      case printing::SIMPLEX:
        cups_duplex_mode = kDuplexNone;
        break;
      default:  // UNKNOWN_DUPLEX_MODE
        NOTREACHED();
        break;
    }
    gtk_print_settings_set(gtk_settings_, kCUPSDuplex, cups_duplex_mode);
  }
#endif

  if (!page_setup_)
    page_setup_ = gtk_page_setup_new();

  if (page_setup_ && !settings->requested_media().IsDefault()) {
    const PrintSettings::RequestedMedia& requested_media =
        settings->requested_media();
    GtkPaperSize* gtk_current_paper_size =
        gtk_page_setup_get_paper_size(page_setup_);
    if (!PaperSizeMatch(gtk_current_paper_size, requested_media,
                        true /*fuzzy_match*/)) {
      GList* gtk_paper_sizes =
          gtk_paper_size_get_paper_sizes(false /*include_custom*/);
      if (gtk_paper_sizes) {
        GtkPaperSize* matching_gtk_paper_size =
            FindPaperSizeMatch(gtk_paper_sizes, requested_media);
        if (matching_gtk_paper_size) {
          VLOG(1) << "Using listed paper size";
          gtk_page_setup_set_paper_size(page_setup_, matching_gtk_paper_size);
        } else {
          VLOG(1) << "Using custom paper size";
          GtkPaperSize* custom_size = gtk_paper_size_new_custom(
              requested_media.vendor_id.c_str(),
              requested_media.vendor_id.c_str(),
              requested_media.size_microns.width() / kMicronsInMm,
              requested_media.size_microns.height() / kMicronsInMm,
              GTK_UNIT_MM);
          gtk_page_setup_set_paper_size(page_setup_, custom_size);
          gtk_paper_size_free(custom_size);
        }
        g_list_free_full(gtk_paper_sizes,
                         reinterpret_cast<GDestroyNotify>(gtk_paper_size_free));
      }
    } else {
      VLOG(1) << "Using default paper size";
    }
  }

  gtk_print_settings_set_orientation(
      gtk_settings_, settings->landscape() ? GTK_PAGE_ORIENTATION_LANDSCAPE
                                           : GTK_PAGE_ORIENTATION_PORTRAIT);

  InitPrintSettings(std::move(settings));
}

void PrintDialogGtk::ShowDialog(
    gfx::NativeView parent_view,
    bool has_selection,
    PrintingContextLinux::PrintSettingsCallback callback) {
  callback_ = std::move(callback);
  DCHECK(callback_);

  dialog_ = gtk_print_unix_dialog_new(nullptr, nullptr);
  libgtkui::SetGtkTransientForAura(dialog_, parent_view);
  if (parent_view)
    parent_view->AddObserver(this);
  g_signal_connect(dialog_, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

  // Handle the case when the existing |gtk_settings_| has "selection" selected
  // as the page range, but |has_selection| is false.
  if (!has_selection) {
    GtkPrintPages range = gtk_print_settings_get_print_pages(gtk_settings_);
    if (range == GTK_PRINT_PAGES_SELECTION)
      gtk_print_settings_set_print_pages(gtk_settings_, GTK_PRINT_PAGES_ALL);
  }

  // Set modal so user cannot focus the same tab and press print again.
  gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);

  // Since we only generate PDF, only show printers that support PDF.
  // TODO(thestig) Add more capabilities to support?
  GtkPrintCapabilities cap = static_cast<GtkPrintCapabilities>(
      GTK_PRINT_CAPABILITY_GENERATE_PDF |
      GTK_PRINT_CAPABILITY_PAGE_SET |
      GTK_PRINT_CAPABILITY_COPIES |
      GTK_PRINT_CAPABILITY_COLLATE |
      GTK_PRINT_CAPABILITY_REVERSE);
  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(dialog_),
                                                cap);
  gtk_print_unix_dialog_set_embed_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_),
                                             TRUE);
  gtk_print_unix_dialog_set_support_selection(GTK_PRINT_UNIX_DIALOG(dialog_),
                                              TRUE);
  gtk_print_unix_dialog_set_has_selection(GTK_PRINT_UNIX_DIALOG(dialog_),
                                          has_selection);
  gtk_print_unix_dialog_set_settings(GTK_PRINT_UNIX_DIALOG(dialog_),
                                     gtk_settings_);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_widget_show(dialog_);

  // We need to call gtk_window_present after making the widgets visible to make
  // sure window gets correctly raised and gets focus.
#if defined(USE_X11)
  gtk_window_present_with_time(
      GTK_WINDOW(dialog_), ui::X11EventSource::GetInstance()->GetTimestamp());
#else
  gtk_window_present(GTK_WINDOW(dialog_));
#endif
}

void PrintDialogGtk::PrintDocument(const printing::MetafilePlayer& metafile,
                                   const base::string16& document_name) {
  // This runs on the print worker thread, does not block the UI thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The document printing tasks can outlive the PrintingContext that created
  // this dialog.
  AddRef();

  bool success = base::CreateTemporaryFile(&path_to_pdf_);

  if (success) {
    base::File file;
    file.Initialize(path_to_pdf_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    success = metafile.SaveTo(&file);
    file.Close();
    if (!success)
      base::DeleteFile(path_to_pdf_, false);
  }

  if (!success) {
    LOG(ERROR) << "Saving metafile failed";
    // Matches AddRef() above.
    Release();
    return;
  }

  // No errors, continue printing.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PrintDialogGtk::SendDocumentToPrinter, this,
                                document_name));
}

void PrintDialogGtk::AddRefToDialog() {
  AddRef();
}

void PrintDialogGtk::ReleaseDialog() {
  Release();
}

void PrintDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  int num_matched_handlers = g_signal_handlers_disconnect_by_func(
      dialog_, reinterpret_cast<gpointer>(&OnResponseThunk), this);
  CHECK_EQ(1, num_matched_handlers);

  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      if (gtk_settings_)
        g_object_unref(gtk_settings_);
      gtk_settings_ =
          gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog_));

      if (printer_)
        g_object_unref(printer_);
      printer_ = gtk_print_unix_dialog_get_selected_printer(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(printer_);

      if (page_setup_)
        g_object_unref(page_setup_);
      page_setup_ =
          gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(page_setup_);

      // Handle page ranges.
      PageRanges ranges_vector;
      gint num_ranges;
      bool print_selection_only = false;
      switch (gtk_print_settings_get_print_pages(gtk_settings_)) {
        case GTK_PRINT_PAGES_RANGES: {
          GtkPageRange* gtk_range =
              gtk_print_settings_get_page_ranges(gtk_settings_, &num_ranges);
          if (gtk_range) {
            for (int i = 0; i < num_ranges; ++i) {
              printing::PageRange range;
              range.from = gtk_range[i].start;
              range.to = gtk_range[i].end;
              ranges_vector.push_back(range);
            }
            g_free(gtk_range);
          }
          break;
        }
        case GTK_PRINT_PAGES_SELECTION:
          print_selection_only = true;
          break;
        case GTK_PRINT_PAGES_ALL:
          // Leave |ranges_vector| empty to indicate print all pages.
          break;
        case GTK_PRINT_PAGES_CURRENT:
        default:
          NOTREACHED();
          break;
      }

      auto settings = std::make_unique<PrintSettings>();
      settings->set_is_modifiable(context_->settings().is_modifiable());
      settings->set_ranges(ranges_vector);
      settings->set_selection_only(print_selection_only);
      InitPrintSettingsGtk(gtk_settings_, page_setup_, settings.get());
      context_->InitWithSettings(std::move(settings));
      std::move(callback_).Run(PrintingContextLinux::OK);
      return;
    }
    case GTK_RESPONSE_DELETE_EVENT:  // Fall through.
    case GTK_RESPONSE_CANCEL: {
      std::move(callback_).Run(PrintingContextLinux::CANCEL);
      return;
    }
    case GTK_RESPONSE_APPLY:
    default: { NOTREACHED(); }
  }
}

static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                gpointer user_data,
                                const GError* error
                                ) {
  static_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job, error);
}
void PrintDialogGtk::SendDocumentToPrinter(
    const base::string16& document_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If |printer_| is nullptr then somehow the GTK printer list changed out
  // under us. In which case, just bail out.
  if (!printer_) {
    // Matches AddRef() in PrintDocument();
    Release();
    return;
  }

  // Save the settings for next time.
  GetLastUsedSettings().SetLastUsedSettings(gtk_settings_);

  GtkPrintJob* print_job =
      gtk_print_job_new(base::UTF16ToUTF8(document_name).c_str(), printer_,
                        gtk_settings_, page_setup_);
  gtk_print_job_set_source_file(print_job, path_to_pdf_.value().c_str(),
                                nullptr);
  gtk_print_job_send(print_job, OnJobCompletedThunk, this, nullptr);
}

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* print_job,
                                    const GError* error) {
  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;
  if (print_job)
    g_object_unref(print_job);

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), path_to_pdf_,
                     false));
  // Printing finished. Matches AddRef() in PrintDocument();
  Release();
}

void PrintDialogGtk::InitPrintSettings(
    std::unique_ptr<PrintSettings> settings) {
  InitPrintSettingsGtk(gtk_settings_, page_setup_, settings.get());
  context_->InitWithSettings(std::move(settings));
}

void PrintDialogGtk::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(libgtkui::GetAuraTransientParent(dialog_), window);

  libgtkui::ClearAuraTransientParent(dialog_);
  window->RemoveObserver(this);
  if (callback_)
    std::move(callback_).Run(PrintingContextLinux::CANCEL);
}
