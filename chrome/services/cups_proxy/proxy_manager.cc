// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/proxy_manager.h"

#include <cups/ipp.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/ring_buffer.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/ipp_validator.h"
#include "chrome/services/cups_proxy/printer_installer.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"
#include "chrome/services/cups_proxy/public/cpp/type_conversions.h"
#include "chrome/services/cups_proxy/socket_manager.h"
#include "chrome/services/ipp_parser/public/cpp/browser/ipp_parser_launcher.h"
#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "printing/backend/cups_ipp_helper.h"

namespace cups_proxy {
namespace {

struct InFlightRequest {
  IppRequest request;
  ProxyManager::ProxyRequestCallback cb;
};

class ProxyManagerImpl : public ProxyManager {
 public:
  ProxyManagerImpl(mojo::PendingReceiver<mojom::CupsProxier> request,
                   std::unique_ptr<CupsProxyServiceDelegate> delegate,
                   std::unique_ptr<IppValidator> ipp_validator,
                   std::unique_ptr<PrinterInstaller> printer_installer,
                   std::unique_ptr<SocketManager> socket_manager)
      : delegate_(std::move(delegate)),
        ipp_validator_(std::move(ipp_validator)),
        printer_installer_(std::move(printer_installer)),
        socket_manager_(std::move(socket_manager)),
        receiver_(this, std::move(request)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    receiver_.set_disconnect_handler(
        base::BindOnce([] { LOG(ERROR) << "CupsProxy mojo connection lost"; }));
  }

  ProxyManagerImpl(const ProxyManagerImpl&) = delete;
  ProxyManagerImpl& operator=(const ProxyManagerImpl&) = delete;

  ~ProxyManagerImpl() override;

  void ProxyRequest(const std::string& method,
                    const std::string& url,
                    const std::string& version,
                    const std::vector<ipp_converter::HttpHeader>& headers,
                    const std::vector<uint8_t>& body,
                    ProxyRequestCallback cb) override;

 private:
  // These methods interface with |ipp_parser_| to parse the syntax of the
  // inflight IPP request.
  void ParseIpp();
  void OnParseIpp(ipp_parser::mojom::IppRequestPtr parsed_request);

  // For CUPS-Get-Printers requests, we spoof the response.
  void SpoofGetPrinters();

  // The callback for interfacing with |printer_installer_|; used to
  // pre-install any printers referenced by the inflight request into CUPS.
  void OnInstallPrinter(InstallPrinterResult res);

  // These methods interface with |socket_manager_| to actually proxy the
  // inflight request to CUPS and propagate back its IPP response.
  void ProxyToCups();
  void OnProxyToCups(std::unique_ptr<std::vector<uint8_t>> response);

  // Proxy the IPP response back to the caller.
  void ProxyResponseToCaller(const std::vector<uint8_t>& response);

  // Fail in-flight request.
  void Fail(const std::string& error_message, int http_status_code);

  // Delegate providing necessary Profile dependencies.
  std::unique_ptr<CupsProxyServiceDelegate> delegate_;

  // Current in-flight request.
  std::unique_ptr<InFlightRequest> in_flight_;

  // Timestamp ring buffer.
  base::RingBuffer<base::TimeTicks, kRateLimit> timestamp_;

  // CupsIppParser Service handle.
  mojo::Remote<ipp_parser::mojom::IppParser> ipp_parser_;

  // Runs in current sequence.
  std::unique_ptr<IppValidator> ipp_validator_;
  std::unique_ptr<PrinterInstaller> printer_installer_;
  std::unique_ptr<SocketManager> socket_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<mojom::CupsProxier> receiver_;
  base::WeakPtrFactory<ProxyManagerImpl> weak_factory_{this};
};

std::optional<std::vector<uint8_t>> RebuildIppRequest(
    const std::string& method,
    const std::string& url,
    const std::string& version,
    const std::vector<ipp_converter::HttpHeader>& headers,
    const std::vector<uint8_t>& body) {
  auto request_line_buffer =
      ipp_converter::BuildRequestLine(method, url, version);
  if (!request_line_buffer.has_value()) {
    return std::nullopt;
  }

  auto headers_buffer = ipp_converter::BuildHeaders(headers);
  if (!headers_buffer.has_value()) {
    return std::nullopt;
  }

  std::vector<uint8_t> ret;
  ret.insert(ret.end(), request_line_buffer->begin(),
             request_line_buffer->end());
  ret.insert(ret.end(), headers_buffer->begin(), headers_buffer->end());
  ret.insert(ret.end(), body.begin(), body.end());
  return ret;
}

ProxyManagerImpl::~ProxyManagerImpl() {
  if (in_flight_) {
    Fail("CupsProxy is shutting down", HTTP_STATUS_SERVICE_UNAVAILABLE);
  }
}

void ProxyManagerImpl::ProxyRequest(
    const std::string& method,
    const std::string& url,
    const std::string& version,
    const std::vector<ipp_converter::HttpHeader>& headers,
    const std::vector<uint8_t>& body,
    ProxyRequestCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Limit the rate of requests to kRateLimit per second.
  // The way the algorithm works is if the number of times this method was
  // called between a second ago and now, including the current call, exceeds
  // kRateLimit, the request is blocked and the HTTP 429 Too Many Requests
  // response status code is returned.
  base::TimeTicks time = base::TimeTicks::Now();
  bool block_request = timestamp_.CurrentIndex() >= timestamp_.BufferSize() &&
                       time - timestamp_.ReadBuffer(0) < base::Seconds(1);
  timestamp_.SaveToBuffer(time);
  if (block_request) {
    LOG(WARNING) << "CupsPrintService: Rate limit (" << kRateLimit
                 << ") exceeded";
    std::move(cb).Run({}, {}, 429);  // HTTP_STATUS_TOO_MANY_REQUESTS
    return;
  }

  if (!delegate_->IsPrinterAccessAllowed()) {
    DVLOG(1) << "Printer access not allowed";
    std::move(cb).Run(/*headers=*/{}, /*ipp_message=*/{},
                      HTTP_STATUS_FORBIDDEN);
    return;
  }

  // If we already have an in-flight request, we fail this incoming one
  // directly.
  if (in_flight_) {
    DVLOG(1) << "CupsPrintService Error: Already have an in-flight request";
    std::move(cb).Run({}, {}, HTTP_STATUS_SERVICE_UNAVAILABLE);
    return;
  }

  in_flight_ = std::make_unique<InFlightRequest>();
  in_flight_->cb = std::move(cb);

  // TODO(crbug.com/945409): Rename in both proxy.mojom's: url -> endpoint.
  auto request_buffer = RebuildIppRequest(method, url, version, headers, body);
  if (!request_buffer) {
    return Fail("Failed to rebuild incoming IPP request",
                HTTP_STATUS_SERVER_ERROR);
  }

  // Save request.
  in_flight_->request.buffer = *request_buffer;
  ParseIpp();
  return;
}

void ProxyManagerImpl::ParseIpp() {
  // Launch CupsIppParser service, if needed.
  if (!ipp_parser_) {
    ipp_parser_.Bind(ipp_parser::LaunchIppParser());
    ipp_parser_.reset_on_disconnect();
  }

  // Run out-of-process IPP parsing.
  ipp_parser_->ParseIpp(in_flight_->request.buffer,
                        base::BindOnce(&ProxyManagerImpl::OnParseIpp,
                                       weak_factory_.GetWeakPtr()));
}

void ProxyManagerImpl::OnParseIpp(
    ipp_parser::mojom::IppRequestPtr parsed_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_flight_);

  if (!parsed_request) {
    return Fail("Failed to parse IPP request", HTTP_STATUS_BAD_REQUEST);
  }

  // Validate parsed request.
  auto valid_request =
      ipp_validator_->ValidateIppRequest(std::move(parsed_request));
  if (!valid_request) {
    return Fail("Failed to validate IPP request", HTTP_STATUS_BAD_REQUEST);
  }

  // Save newly validated request.
  in_flight_->request = std::move(*valid_request);

  auto opcode = ippGetOperation(in_flight_->request.ipp.get());
  if (opcode == IPP_OP_CUPS_NONE) {
    return Fail("Failed to parse IPP operation ID", HTTP_STATUS_BAD_REQUEST);
  }

  // Since Chrome is the source-of-truth for printers on ChromeOS, for
  // CUPS-Get-Printers requests, we spoof the response rather than proxying to
  // CUPS.
  if (opcode == IPP_OP_CUPS_GET_PRINTERS) {
    SpoofGetPrinters();
    return;
  }

  // If this request references a printer, pre-install it into CUPS.
  auto printer_uuid = GetPrinterId(in_flight_->request.ipp.get());
  if (printer_uuid.has_value()) {
    printer_installer_->InstallPrinter(
        *printer_uuid, base::BindOnce(&ProxyManagerImpl::OnInstallPrinter,
                                      weak_factory_.GetWeakPtr()));
    return;
  }

  // Nothing left to do, skip straight to proxying.
  ProxyToCups();
  return;
}

void ProxyManagerImpl::SpoofGetPrinters() {
  std::vector<chromeos::Printer> printers = FilterPrintersForPluginVm(
      delegate_->GetPrinters(chromeos::PrinterClass::kSaved),
      delegate_->GetPrinters(chromeos::PrinterClass::kEnterprise),
      delegate_->GetRecentlyUsedPrinters());
  std::optional<IppResponse> response =
      BuildGetDestsResponse(in_flight_->request, printers);
  if (!response.has_value()) {
    return Fail("Failed to spoof CUPS-Get-Printers response",
                HTTP_STATUS_SERVER_ERROR);
  }

  ProxyResponseToCaller(response->buffer);
  return;
}

void ProxyManagerImpl::OnInstallPrinter(InstallPrinterResult res) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_flight_);

  if (res != InstallPrinterResult::kSuccess) {
    return Fail("Failed to pre-install printer", HTTP_STATUS_SERVER_ERROR);
  }

  ProxyToCups();
  return;
}

void ProxyManagerImpl::ProxyToCups() {
  // Queue request with socket_manager_
  socket_manager_->ProxyToCups(std::move(in_flight_->request.buffer),
                               base::BindOnce(&ProxyManagerImpl::OnProxyToCups,
                                              weak_factory_.GetWeakPtr()));
}

void ProxyManagerImpl::OnProxyToCups(
    std::unique_ptr<std::vector<uint8_t>> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_flight_);

  if (!response) {
    return Fail("Failed to proxy request", HTTP_STATUS_SERVER_ERROR);
  }

  ProxyResponseToCaller(*response);
  return;
}

void ProxyManagerImpl::ProxyResponseToCaller(
    const std::vector<uint8_t>& response) {
  // Convert to string for parsing HTTP headers.
  auto end_of_headers = net::HttpUtil::LocateEndOfHeaders(response);
  if (end_of_headers < 0) {
    return Fail("IPP response missing end of headers",
                HTTP_STATUS_SERVER_ERROR);
  }
  std::string_view headers_slice =
      base::as_string_view(base::span(response).first(end_of_headers));
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(headers_slice);
  if (!response_headers) {
    return Fail("Failed to parse HTTP response headers",
                HTTP_STATUS_SERVER_ERROR);
  }

  std::vector<ipp_converter::HttpHeader> parsed_headers;
  size_t iter = 0;
  std::string name, value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    parsed_headers.push_back({name, value});
  }

  // Slice off the ipp_message as is.
  std::vector<uint8_t> ipp_message(response.begin() + end_of_headers,
                                   response.end());

  // Send parsed response back to caller.
  std::move(in_flight_->cb)
      .Run(std::move(parsed_headers), std::move(ipp_message), HTTP_STATUS_OK);
  in_flight_.reset();
}

// TODO(crbug.com/945409): Fail with comprehensive HTTP Error response.
// Fails current request by running its callback with an empty response and
// clearing in_flight_.
void ProxyManagerImpl::Fail(const std::string& error_message,
                            int http_status_code) {
  DCHECK(in_flight_);

  DVLOG(1) << "CupsPrintService Error: " << error_message;

  std::move(in_flight_->cb).Run({}, {}, http_status_code);
  in_flight_.reset();
}

}  // namespace

// static
std::unique_ptr<ProxyManager> ProxyManager::Create(
    mojo::PendingReceiver<mojom::CupsProxier> request,
    std::unique_ptr<CupsProxyServiceDelegate> delegate) {
  auto* delegate_ptr = delegate.get();
  return std::make_unique<ProxyManagerImpl>(
      std::move(request), std::move(delegate),
      std::make_unique<IppValidator>(delegate_ptr),
      std::make_unique<PrinterInstaller>(delegate_ptr),
      SocketManager::Create(delegate_ptr));
}

std::unique_ptr<ProxyManager> ProxyManager::CreateForTesting(
    mojo::PendingReceiver<mojom::CupsProxier> request,
    std::unique_ptr<CupsProxyServiceDelegate> delegate,
    std::unique_ptr<IppValidator> ipp_validator,
    std::unique_ptr<PrinterInstaller> printer_installer,
    std::unique_ptr<SocketManager> socket_manager) {
  return std::make_unique<ProxyManagerImpl>(
      std::move(request), std::move(delegate), std::move(ipp_validator),
      std::move(printer_installer), std::move(socket_manager));
}

}  // namespace cups_proxy
