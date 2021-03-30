# dPWA Integration Tests

## Overview
The dPWA integration tests use a special framework. Each test is defined by a
series of "testing actions", and test cases are read from a csv file.

### Identifying and Diagnosing Failed Tests

Every test failure will log a message that will give:

 * the failing test case
 * the failing action
 * the line to add to the TestExpectations file to disable the test
 * the command line argument to specify to run the given test locally

### Test Input Files

Test files live in //chrome/browser/test/data/web_apps/:
 * web_app_integration_browsertest_cases.csv
 * web_app_integration_browsertest_cases_sync.csv
 * TestExpectations

### Disabling a Test

To disable a failing / crashing test, add an entry to the TestExpectations
file mentioned above. The format is as follows:
```
crbug.com/id [ Platform ] [ Expectation ] list,of,actions,in,test
```

The list of supported platforms and expectations is maintained in the
TestExpectations file. This test suite requires adding an entry
per-platform that the test should be disabled on. Please create a bug for each
test case added to this file.

## How It Works

The way in which this works is by using a script to generate a minimal set of
test cases that produce the maximum amount of code coverage, and reading in that
script output in these test implementations.


This suite of tests has two main parts:
 1. A script that analyzes dPWA code ([See design
doc](https://docs.google.com/document/d/1YmeNZCpIwUbeV3K3HGUdXzJjZDKIDyKrGfyjnYaLR5k).
 2. The test implementations ([See design
doc](https://docs.google.com/document/d/1Gd14fjwA4VKoRzL2TAvi9paXwyh36ehlS4gbpUmUeeI).

### Script
The [python script](https://crrev.com/c/2459059) takes (among other things)
as input:
 * A list of all testing actions
 * A list of testing actions currently supported by the testing framework
 * A list of all testing journeys that we need coverage for

These lists will be checked in via tsv files with the above-linked CL, but the
source of truth lives in this [spreadsheet](https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ).
See "Test Input Files" section for location of test case files.

The testing actions represent actions that users can take that trigger dPWA
code, such as navigating to an installable site, or installing a PWA. Testing
actions can also represent things such as setting up an enterprise policy
to install an app. The script will generate a coverage graph, prune the
graph to only include actions currently supported by the framework, and it will
generate a test case definition csv file with the goal of minimizing the
number of test cases to get maximum coverage.

### Test implementation
The high level flow of execution is as follows:
 * Read the input file which contains the test cases
 * Parse the test cases into an `std::vector<std::vector<std::string>>`
 * Pass the vector of test cases to a parameterized test using
   `testing::ValuesIn()`, which will run a test for each line in the input
   file
 * Each test will loop over the testing actions, calling
   `ExecuteAction(action_string)`
 * `ExecuteAction()` will switch on the string, and call the appropriate
   action implementation method
 * A state snapshot will be captured after non-inspection (state mutating)
   actions (so inspection actions can assert various state changes)


## Components
[Design
doc](https://docs.google.com/document/d/139ktCajbmbFKh4T-vEhipTxilyYrXf_rlCBHIvrdeSg).

### WebAppIntegrationBrowserTestBase
A helper class containing most of the test implementation, meant to be used
as a private member on the test-driving classes. Contains most of the test
implementation:
 * Input file parser
 * ExecuteAction()
 * Most action implementation methods
 * Capturing state snapshots

### WebAppIntegrationBrowserTestBase::TestDelegate
An abstract class that’s an application of the delegate interface pattern.
`WebAppIntegrationBrowserTestBase` stores an instance of this class as a
private member, `delegate_`, allowing the base class to call into protected
members of `InProcessBrowserTest` or `SyncTest`, such as
`InProcessBrowserTest::browser()` or `SyncTest::GetAllProfiles()`. This also
has pure virtual methods for sync functionality that needs to be implemented
in `TwoClientWebAppsSyncTest`, but called from the base class.

### WebAppIntegrationBrowserTest
Subclass of both `InProcessBrowserTest` and
`WebAppIntegrationBrowserTestBase::TestDelegate`. Drives the test by
calling `IN_PROC_BROWSER_TEST_P` and instantiating the parameterized
test as described above. Responsible for telling the base class where the test
input files live, handling test setup, and implementing `TestDelegate` methods
to expose protected members of `InProcessBrowserTest` to the base class. This
class owns the base class, and stores it as a private member, `helper_`, passing
it an instance of itself (as the TestDelegate) on construction.


### TwoClientWebAppsSyncTest
Similar to `WebAppIntegrationBrowserTest`, but inheriting from `SyncTest`
instead of from `InProcessBrowserTest`. In addition, some testing actions
related to profile sync are implemented in this class, and are exposed via
`TestDelegate` pure virtual method overrides.

