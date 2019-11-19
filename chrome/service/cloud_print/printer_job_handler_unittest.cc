// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/cloud_print/print_system.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "printing/backend/print_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace cloud_print {

namespace {

using base::StringPrintf;

const char kExampleCloudPrintServerURL[] = "https://www.google.com/cloudprint/";

const char kExamplePrintTicket[] = "{\"MediaType\":\"plain\","
    "\"Resolution\":\"300x300dpi\",\"PageRegion\":\"Letter\","
    "\"InputSlot\":\"auto\",\"PageSize\":\"Letter\",\"EconoMode\":\"off\"}";


// The fillowing constants will all be constructed with StringPrintf. The
// following types of parameters are possible:
// job number(int): ID # of job from given job list. All job IDs follow the
// format __example_job_idN for some N.
// fetch reason(string): Fetch reason used by the code. The job list URL
// requested by PrinterJobHandler has an extra parameter that signifies when
// the request was triggered.
// status string(string): Status of print job, one of IN_PROGRESS, DONE or ERROR
// job object list(string/JSON formatted): a comma-separated list of job objects

// StringPrintf parameters: job number, job number, job number, job number
const char kExampleJobObject[] = "{"
"   \"tags\": ["
"    \"^own\""
"   ],"
"   \"printerName\": \"Example Printer\","
"   \"status\": \"QUEUED\","
"   \"ownerId\": \"sampleuser@gmail.com\","
"   \"ticketUrl\": \"https://www.google.com/cloudprint/ticket?exampleURI%d\","
"   \"printerid\": \"__example_printer_id\","
"   \"printerType\": \"GOOGLE\","
"   \"contentType\": \"text/html\","
"   \"fileUrl\": \"https://www.google.com/cloudprint/download?exampleURI%d\","
"   \"id\": \"__example_job_id%d\","
"   \"message\": \"\","
"   \"title\": \"Example Job %d\","
"   \"errorCode\": \"\","
"   \"numberOfPages\": 3"
"  }";

// StringPrintf parameters: job object list
const char kExampleJobListResponse[] = "{"
" \"success\": true,"
" \"jobs\": ["
" %s"
" ],"
" \"xsrf_token\": \"AIp06DjUd3AV6BO0aujB9NvM2a9ZbogxOQ:1360021066932\","
" \"request\": {"
"  \"time\": \"0\","
"  \"users\": ["
"   \"sampleuser@gmail.com\""
"  ],"
"  \"params\": {"
"   \"printerid\": ["
"    \"__example_printer_id\""
"   ]"
"  },"
"  \"user\": \"sampleuser@gmail.com\""
" }"
"}";


// StringPrintf parameters: job number
const char kExampleJobID[] = "__example_job_id%d";

// StringPrintf parameters: job number
const char kExamplePrintTicketURI[] =
    "https://www.google.com/cloudprint/ticket?exampleURI%d";

// StringPrintf parameters: job number
const char kExamplePrintDownloadURI[] =
    "https://www.google.com/cloudprint/download?exampleURI%d";

// StringPrintf parameters: job number
const char kExampleUpdateDoneURI[] =
    "https://www.google.com/cloudprint/control?jobid=__example_job_id%d"
    "&status=DONE&code=0&message=&numpages=0&pagesprinted=0";

// StringPrintf parameters: job number
const char kExampleUpdateErrorURI[] =
    "https://www.google.com/cloudprint/control?jobid=__example_job_id%d"
    "&status=ERROR";

// StringPrintf parameters: fetch reason
const char kExamplePrinterJobListURI[] =
    "https://www.google.com/cloudprint/fetch"
    "?printerid=__example_printer_id&deb=%s";

// StringPrintf parameters: status string, job number, status string (repeat)
const char kExampleControlResponse[] = "{"
" \"success\": true,"
" \"message\": \"Print job updated successfully.\","
" \"xsrf_token\": \"AIp06DjKgbfGalbqzj23V1bU6i-vtR2B4w:1360023068789\","
" \"request\": {"
"  \"time\": \"0\","
"  \"users\": ["
"   \"sampleuser@gmail.com\""
"  ],"
"  \"params\": {"
"   \"xsrf\": ["
"    \"AIp06DgeGIETs42Cj28QWmxGPWVDiaXwVQ:1360023041852\""
"   ],"
"   \"status\": ["
"    \"%s\""
"   ],"
"   \"jobid\": ["
"    \"__example_job_id%d\""
"   ]"
"  },"
"  \"user\": \"sampleuser@gmail.com\""
" },"
" \"job\": {"
"  \"tags\": ["
"   \"^own\""
"  ],"
"  \"printerName\": \"Example Printer\","
"  \"status\": \"%s\","
"  \"ownerId\": \"sampleuser@gmail.com\","
"  \"ticketUrl\": \"https://www.google.com/cloudprint/ticket?exampleURI1\","
"  \"printerid\": \"__example_printer_id\","
"  \"contentType\": \"text/html\","
"  \"fileUrl\": \"https://www.google.com/cloudprint/download?exampleURI1\","
"  \"id\": \"__example_job_id1\","
"  \"message\": \"\","
"  \"title\": \"Example Job\","
"  \"errorCode\": \"\","
"  \"numberOfPages\": 3"
" }"
"}";

const char kExamplePrinterID[] = "__example_printer_id";

const char kExamplePrinterCapabilities[] = "";

const char kExampleCapsMimeType[] = "";

// These can stay empty
const char kExampleDefaults[] = "";

const char kExampleDefaultMimeType[] = "";

// Since we're not connecting to the server, this can be any non-empty string.
const char kExampleCloudPrintOAuthToken[] = "__SAMPLE_TOKEN";


// Not actually printing, no need for real PDF.
const char kExamplePrintData[] = "__EXAMPLE_PRINT_DATA";

const char kExampleJobDownloadResponseHeaders[] =
    "Content-Type: Application/PDF\n";

const char kExampleTicketDownloadResponseHeaders[] =
    "Content-Type: application/json\n";

const char kExamplePrinterName[] = "Example Printer";

const char kExamplePrinterDescription[] = "Example Description";

// These are functions used to construct the various sample strings.
std::string JobListResponse(int num_jobs) {
  std::string job_objects;
  for (int i = 0; i < num_jobs; i++) {
    job_objects = job_objects + StringPrintf(kExampleJobObject, i+1, i+1, i+1,
                                             i+1);
    if (i != num_jobs-1) job_objects = job_objects + ",";
  }
  return StringPrintf(kExampleJobListResponse, job_objects.c_str());
}

GURL JobListURI(const char* reason) {
  return GURL(StringPrintf(kExamplePrinterJobListURI, reason));
}

GURL DoneURI(int job_num) {
  return GURL(StringPrintf(kExampleUpdateDoneURI, job_num));
}

GURL ErrorURI(int job_num) {
  return GURL(StringPrintf(kExampleUpdateErrorURI, job_num));
}

GURL TicketURI(int job_num) {
  return GURL(StringPrintf(kExamplePrintTicketURI, job_num));
}

GURL DownloadURI(int job_num) {
  return GURL(StringPrintf(kExamplePrintDownloadURI, job_num));
}

GURL InProgressURI(int job_num) {
  return GetUrlForJobStatusUpdate(GURL(kExampleCloudPrintServerURL),
                                  StringPrintf(kExampleJobID, job_num),
                                  PRINT_JOB_STATUS_IN_PROGRESS,
                                  0);
}

std::string StatusResponse(int job_num, const char* status_string) {
  return StringPrintf(kExampleControlResponse,
                      status_string,
                      job_num,
                      status_string);
}

}  // namespace

class CloudPrintURLFetcherNoServiceProcess
    : public CloudPrintURLFetcher {
 public:
  CloudPrintURLFetcherNoServiceProcess()
      : CloudPrintURLFetcher(PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS),
        context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())) {}

 protected:
  net::URLRequestContextGetter* GetRequestContextGetter() override {
    return context_getter_.get();
  }

  ~CloudPrintURLFetcherNoServiceProcess() override {}

 private:
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
};


class CloudPrintURLFetcherNoServiceProcessFactory
    : public CloudPrintURLFetcherFactory {
 public:
  CloudPrintURLFetcher* CreateCloudPrintURLFetcher() override {
    return new CloudPrintURLFetcherNoServiceProcess;
  }

  ~CloudPrintURLFetcherNoServiceProcessFactory() override {}
};


// This class handles the callback from FakeURLFetcher
// It is a separate class because callback methods must be
// on RefCounted classes

class TestURLFetcherCallback {
 public:
  std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* d,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    std::unique_ptr<net::FakeURLFetcher> fetcher(
        new net::FakeURLFetcher(url, d, response_data, response_code, status));
    OnRequestCreate(url, fetcher.get());
    return fetcher;
  }
  MOCK_METHOD2(OnRequestCreate,
               void(const GURL&, net::FakeURLFetcher*));
};


class MockPrinterJobHandlerDelegate
    : public PrinterJobHandler::Delegate {
 public:
  MOCK_METHOD0(OnAuthError, void());
  MOCK_METHOD1(OnPrinterDeleted, void(const std::string& str));

  ~MockPrinterJobHandlerDelegate() override {}
};


class MockPrintServerWatcher
    : public PrintSystem::PrintServerWatcher {
 public:
  MOCK_METHOD1(StartWatching,
               bool(PrintSystem::PrintServerWatcher::Delegate* d));
  MOCK_METHOD0(StopWatching, bool());

  MockPrintServerWatcher();
  PrintSystem::PrintServerWatcher::Delegate* delegate() const {
    return delegate_;
  }

  friend class scoped_refptr<NiceMock<MockPrintServerWatcher> >;
  friend class scoped_refptr<StrictMock<MockPrintServerWatcher> >;
  friend class scoped_refptr<MockPrintServerWatcher>;

 protected:
  ~MockPrintServerWatcher() override {}

 private:
  PrintSystem::PrintServerWatcher::Delegate* delegate_;
};

class MockPrinterWatcher : public PrintSystem::PrinterWatcher {
 public:
  MOCK_METHOD1(StartWatching, bool(PrintSystem::PrinterWatcher::Delegate* d));
  MOCK_METHOD0(StopWatching, bool());
  MOCK_METHOD1(GetCurrentPrinterInfo,
               bool(printing::PrinterBasicInfo* printer_info));

  MockPrinterWatcher();
  PrintSystem::PrinterWatcher::Delegate* delegate() const { return delegate_; }

  friend class scoped_refptr<NiceMock<MockPrinterWatcher> >;
  friend class scoped_refptr<StrictMock<MockPrinterWatcher> >;
  friend class scoped_refptr<MockPrinterWatcher>;

 protected:
  ~MockPrinterWatcher() override {}

 private:
  PrintSystem::PrinterWatcher::Delegate* delegate_;
};


class MockJobSpooler : public PrintSystem::JobSpooler {
 public:
  MOCK_METHOD8(Spool, bool(
      const std::string& print_ticket,
      const std::string& print_ticket_mime_type,
      const base::FilePath& print_data_file_path,
      const std::string& print_data_mime_type,
      const std::string& printer_name,
      const std::string& job_title,
      const std::vector<std::string>& tags,
      PrintSystem::JobSpooler::Delegate* delegate));

  MockJobSpooler();
  PrintSystem::JobSpooler::Delegate* delegate() const  { return delegate_; }

  friend class scoped_refptr<NiceMock<MockJobSpooler> >;
  friend class scoped_refptr<StrictMock<MockJobSpooler> >;
  friend class scoped_refptr<MockJobSpooler>;

 protected:
  ~MockJobSpooler() override {}

 private:
  PrintSystem::JobSpooler::Delegate* delegate_;
};



class MockPrintSystem : public PrintSystem {
 public:
  MockPrintSystem();
  PrintSystem::PrintSystemResult succeed() {
    return PrintSystem::PrintSystemResult(true, "success");
  }

  PrintSystem::PrintSystemResult fail() {
    return PrintSystem::PrintSystemResult(false, "failure");
  }

  MockJobSpooler& JobSpooler() { return *job_spooler_.get(); }

  MockPrinterWatcher& PrinterWatcher() { return *printer_watcher_.get(); }

  MockPrintServerWatcher& PrintServerWatcher() {
    return *print_server_watcher_.get();
  }

  MOCK_METHOD0(Init, PrintSystem::PrintSystemResult());
  MOCK_METHOD1(EnumeratePrinters, PrintSystem::PrintSystemResult(
      printing::PrinterList* printer_list));

  MOCK_METHOD2(GetPrinterCapsAndDefaults,
               void(const std::string& printer_name,
                    PrintSystem::PrinterCapsAndDefaultsCallback callback));

  MOCK_METHOD1(IsValidPrinter, bool(const std::string& printer_name));

  MOCK_METHOD3(ValidatePrintTicket,
               bool(const std::string& printer_name,
                    const std::string& print_ticket_data,
                    const std::string& print_ticket_mime_type));

  MOCK_METHOD3(GetJobDetails, bool(const std::string& printer_name,
                                    PlatformJobId job_id,
                                    PrintJobDetails* job_details));

  MOCK_METHOD0(CreatePrintServerWatcher, PrintSystem::PrintServerWatcher*());
  MOCK_METHOD1(CreatePrinterWatcher,
               PrintSystem::PrinterWatcher*(const std::string& printer_name));
  MOCK_METHOD0(CreateJobSpooler, PrintSystem::JobSpooler*());

  MOCK_METHOD0(UseCddAndCjt, bool());
  MOCK_METHOD0(GetSupportedMimeTypes, std::string());

  friend class scoped_refptr<NiceMock<MockPrintSystem> >;
  friend class scoped_refptr<StrictMock<MockPrintSystem> >;
  friend class scoped_refptr<MockPrintSystem>;

 protected:
  ~MockPrintSystem() override {}

 private:
  scoped_refptr<MockJobSpooler> job_spooler_;
  scoped_refptr<MockPrinterWatcher> printer_watcher_;
  scoped_refptr<MockPrintServerWatcher> print_server_watcher_;
};


class PrinterJobHandlerTest : public ::testing::Test {
 public:
  PrinterJobHandlerTest();
  void SetUp() override;
  void TearDown() override;
  bool GetPrinterInfo(printing::PrinterBasicInfo* info);
  void SendCapsAndDefaults(
      const std::string& printer_name,
      PrintSystem::PrinterCapsAndDefaultsCallback callback);
  void AddMimeHeader(const GURL& url, net::FakeURLFetcher* fetcher);
  void AddTicketMimeHeader(const GURL& url, net::FakeURLFetcher* fetcher);
  bool PostSpoolSuccess();
  void SetUpJobSuccessTest(int job_num);
  void BeginTest(int timeout_seconds);
  void MakeJobFetchReturnNoJobs();

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::OnceClosure active_run_loop_quit_closure_;
  TestURLFetcherCallback url_callback_;
  MockPrinterJobHandlerDelegate jobhandler_delegate_;
  CloudPrintTokenStore token_store_;
  CloudPrintURLFetcherNoServiceProcessFactory cloud_print_factory_;
  scoped_refptr<PrinterJobHandler> job_handler_;
  scoped_refptr<NiceMock<MockPrintSystem> > print_system_;
  net::FakeURLFetcherFactory factory_;
  printing::PrinterBasicInfo basic_info_;
  printing::PrinterCapsAndDefaults caps_and_defaults_;
  PrinterJobHandler::PrinterInfoFromCloud info_from_cloud_;
};


void PrinterJobHandlerTest::SetUp() {
  basic_info_.printer_name = kExamplePrinterName;
  basic_info_.printer_description = kExamplePrinterDescription;
  basic_info_.is_default = 0;

  info_from_cloud_.printer_id = kExamplePrinterID;
  info_from_cloud_.tags_hash = GetHashOfPrinterInfo(basic_info_);

  info_from_cloud_.caps_hash = base::MD5String(kExamplePrinterCapabilities);
  info_from_cloud_.current_xmpp_timeout = 300;
  info_from_cloud_.pending_xmpp_timeout = 0;

  caps_and_defaults_.printer_capabilities = kExamplePrinterCapabilities;
  caps_and_defaults_.caps_mime_type = kExampleCapsMimeType;
  caps_and_defaults_.printer_defaults = kExampleDefaults;
  caps_and_defaults_.defaults_mime_type = kExampleDefaultMimeType;

  print_system_ = new NiceMock<MockPrintSystem>();

  token_store_.SetToken(kExampleCloudPrintOAuthToken);

  ON_CALL(print_system_->PrinterWatcher(), GetCurrentPrinterInfo(_))
      .WillByDefault(Invoke(this, &PrinterJobHandlerTest::GetPrinterInfo));

  ON_CALL(*print_system_.get(), GetPrinterCapsAndDefaults(_, _))
      .WillByDefault(Invoke(this, &PrinterJobHandlerTest::SendCapsAndDefaults));

  CloudPrintURLFetcher::set_test_factory(&cloud_print_factory_);
}

void PrinterJobHandlerTest::MakeJobFetchReturnNoJobs() {
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonStartup),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonFailure),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonRetry),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
}

PrinterJobHandlerTest::PrinterJobHandlerTest()
    : factory_(nullptr,
               base::BindRepeating(&TestURLFetcherCallback::CreateURLFetcher,
                                   base::Unretained(&url_callback_))) {}

bool PrinterJobHandlerTest::PostSpoolSuccess() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterJobHandler::OnJobSpoolSucceeded, job_handler_, 0));

  // Everything that would be posted on the printer thread queue
  // has been posted, we can tell the main message loop to quit when idle
  // and not worry about it idling while the print thread does work.
  DCHECK(!active_run_loop_quit_closure_.is_null());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(active_run_loop_quit_closure_));
  return true;
}

void PrinterJobHandlerTest::AddMimeHeader(const GURL& url,
                                          net::FakeURLFetcher* fetcher) {
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders(kExampleJobDownloadResponseHeaders);
  fetcher->set_response_headers(download_headers);
}

void PrinterJobHandlerTest::AddTicketMimeHeader(const GURL& url,
                                                net::FakeURLFetcher* fetcher) {
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders(kExampleTicketDownloadResponseHeaders);
  fetcher->set_response_headers(download_headers);
}


void PrinterJobHandlerTest::SetUpJobSuccessTest(int job_num) {
  factory_.SetFakeResponse(TicketURI(job_num),
                           kExamplePrintTicket, net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(DownloadURI(job_num),
                           kExamplePrintData, net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  factory_.SetFakeResponse(DoneURI(job_num),
                           StatusResponse(job_num, "DONE"),
                           net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(InProgressURI(job_num),
                           StatusResponse(job_num, "IN_PROGRESS"),
                           net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  // The times requirement is relaxed for the ticket URI
  // in order to accommodate TicketDownloadFailureTest
  EXPECT_CALL(url_callback_, OnRequestCreate(TicketURI(job_num), _))
      .Times(AtLeast(1))
      .WillOnce(Invoke(this, &PrinterJobHandlerTest::AddTicketMimeHeader));

  EXPECT_CALL(url_callback_, OnRequestCreate(DownloadURI(job_num), _))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &PrinterJobHandlerTest::AddMimeHeader));

  EXPECT_CALL(url_callback_, OnRequestCreate(InProgressURI(job_num), _))
      .Times(Exactly(1));

  EXPECT_CALL(url_callback_, OnRequestCreate(DoneURI(job_num), _))
      .Times(Exactly(1));

  EXPECT_CALL(print_system_->JobSpooler(),
              Spool(kExamplePrintTicket, _, _, _, _, _, _, _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this,
                                  &PrinterJobHandlerTest::PostSpoolSuccess));
}

void PrinterJobHandlerTest::BeginTest(int timeout_seconds) {
  job_handler_ = new PrinterJobHandler(basic_info_,
                                       info_from_cloud_,
                                       GURL(kExampleCloudPrintServerURL),
                                       print_system_.get(),
                                       &jobhandler_delegate_);

  job_handler_->Initialize();

  base::RunLoop run_loop;
  active_run_loop_quit_closure_ = run_loop.QuitWhenIdleClosure();

  base::RunLoop::ScopedRunTimeoutForTest run_timeout(
      base::TimeDelta::FromSeconds(timeout_seconds),
      base::BindLambdaForTesting([&]() {
        ADD_FAILURE();
        run_loop.QuitWhenIdle();
      }));

  run_loop.Run();
}

void PrinterJobHandlerTest::SendCapsAndDefaults(
    const std::string& printer_name,
    PrintSystem::PrinterCapsAndDefaultsCallback callback) {
  std::move(callback).Run(true, printer_name, caps_and_defaults_);
}

bool PrinterJobHandlerTest::GetPrinterInfo(printing::PrinterBasicInfo* info) {
  *info = basic_info_;
  return true;
}

void PrinterJobHandlerTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  CloudPrintURLFetcher::set_test_factory(nullptr);
}

MockPrintServerWatcher::MockPrintServerWatcher() : delegate_(NULL) {
  ON_CALL(*this, StartWatching(_))
      .WillByDefault(DoAll(SaveArg<0>(&delegate_), Return(true)));
  ON_CALL(*this, StopWatching()).WillByDefault(Return(true));
}


MockPrinterWatcher::MockPrinterWatcher() : delegate_(NULL) {
  ON_CALL(*this, StartWatching(_))
      .WillByDefault(DoAll(SaveArg<0>(&delegate_), Return(true)));
  ON_CALL(*this, StopWatching()).WillByDefault(Return(true));
}

MockJobSpooler::MockJobSpooler() : delegate_(NULL) {
  ON_CALL(*this, Spool(_, _, _, _, _, _, _, _))
      .WillByDefault(DoAll(SaveArg<7>(&delegate_), Return(true)));
}

MockPrintSystem::MockPrintSystem()
    : job_spooler_(new NiceMock<MockJobSpooler>()),
      printer_watcher_(new NiceMock<MockPrinterWatcher>()),
      print_server_watcher_(new NiceMock<MockPrintServerWatcher>()) {
  ON_CALL(*this, CreateJobSpooler()).WillByDefault(Return(job_spooler_.get()));

  ON_CALL(*this, CreatePrinterWatcher(_))
      .WillByDefault(Return(printer_watcher_.get()));

  ON_CALL(*this, CreatePrintServerWatcher())
      .WillByDefault(Return(print_server_watcher_.get()));

  ON_CALL(*this, IsValidPrinter(_)).
      WillByDefault(Return(true));

  ON_CALL(*this, ValidatePrintTicket(_, _, _)).
      WillByDefault(Return(true));
}

// This test simulates an end-to-end printing of a document
// but tests only non-failure cases.
// Disabled - http://crbug.com/184245
TEST_F(PrinterJobHandlerTest, DISABLED_HappyPathTest) {
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonStartup),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonQueryMore),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonStartup), _))
      .Times(Exactly(1));
  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonQueryMore), _))
      .Times(Exactly(1));

  SetUpJobSuccessTest(1);
  BeginTest(20);
}

TEST_F(PrinterJobHandlerTest, TicketDownloadFailureTest) {
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonStartup),
                           JobListResponse(2), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonFailure),
                           JobListResponse(2), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonQueryMore),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(TicketURI(1), std::string(),
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);

  EXPECT_CALL(url_callback_, OnRequestCreate(TicketURI(1), _))
      .Times(AtLeast(1))
      .WillOnce(Invoke(this, &PrinterJobHandlerTest::AddTicketMimeHeader));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonStartup), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonQueryMore), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonFailure), _))
      .Times(AtLeast(1));

  SetUpJobSuccessTest(2);
  BeginTest(20);
}

// TODO(noamsml): Figure out how to make this test not take 1 second and
// re-enable it
TEST_F(PrinterJobHandlerTest, DISABLED_ManyFailureTest) {
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonStartup),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonFailure),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonRetry),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonQueryMore),
                           JobListResponse(0), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonStartup), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonQueryMore), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonFailure), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonRetry), _))
      .Times(AtLeast(1));

  SetUpJobSuccessTest(1);

  factory_.SetFakeResponse(TicketURI(1),
                           std::string(),
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);

  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&net::FakeURLFetcherFactory::SetFakeResponse,
                     base::Unretained(&factory_), TicketURI(1),
                     kExamplePrintTicket, net::HTTP_OK,
                     net::URLRequestStatus::SUCCESS),
      base::TimeDelta::FromSeconds(1));

  BeginTest(5);
}


// TODO(noamsml): Figure out how to make this test not take ~64-~2048 (depending
// constant values) seconds and re-enable it
TEST_F(PrinterJobHandlerTest, DISABLED_CompleteFailureTest) {
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonStartup),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonFailure),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(JobListURI(kJobFetchReasonRetry),
                           JobListResponse(1), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(ErrorURI(1), StatusResponse(1, "ERROR"),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(TicketURI(1), std::string(),
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonStartup), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonFailure), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_,
              OnRequestCreate(JobListURI(kJobFetchReasonRetry), _))
      .Times(AtLeast(1));

  EXPECT_CALL(url_callback_, OnRequestCreate(ErrorURI(1), _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(
          this, &PrinterJobHandlerTest::MakeJobFetchReturnNoJobs));

  EXPECT_CALL(url_callback_, OnRequestCreate(TicketURI(1), _))
      .Times(AtLeast(kNumRetriesBeforeAbandonJob));

  BeginTest(70);
}

}  // namespace cloud_print
