// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generator script for creating gtest-style JavaScript
 *     tests for extensions, WebUI and unit tests. Generates C++ gtest wrappers
 *     which will invoke the appropriate JavaScript for each test.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see WebUI testing: http://goo.gl/ZWFXF
 * @see gtest documentation: http://goo.gl/Ujj3H
 * @see chrome/chrome_tests.gypi
 * @see chrome/test/base/js2gtest.js
 * @see tools/gypv8sh.py
 */

// Arguments from rules in chrome_tests.gypi are passed in through
// python script gypv8sh.py.
if (arguments.length != 6) {
  print('usage: ' +
        arguments[0] +
        ' path-to-testfile.js testfile.js path_to_deps.js output.cc test-type');
  quit(-1);
}

/**
 * Full path to the test input file.
 * @type {string}
 */
var jsFile = arguments[1];

/**
 * Relative path to the test input file appropriate for use in the
 * C++ TestFixture's addLibrary method.
 * @type {string}
 */
var jsFileBase = arguments[2];

/**
 * Path to Closure library style deps.js file.
 * @type {string?}
 */
var depsFile = arguments[3];

/**
 * Path to C++ file generation is outputting to.
 * @type {string}
 */
var outputFile = arguments[4];

/**
 * Type of this test.
 * @type {string} ('extension' | 'unit' | 'webui')
 */
var testType = arguments[5];
if (testType != 'extension' &&
    testType != 'unit' &&
    testType != 'webui' &&
    testType != 'mojo_webui') {
  print('Invalid test type: ' + testType);
  quit(-1);
}

/**
 * C++ gtest macro to use for TEST_F depending on |testType|.
 * @type {string} ('TEST_F'|'IN_PROC_BROWSER_TEST_F')
 */
var testF;

/**
 * Keeps track of whether a typedef has been generated for each test
 * fixture.
 * @type {Object<string>}
 */
var typedeffedCppFixtures = {};

/**
 * Maintains a list of relative file paths to add to each gtest body
 * for inclusion at runtime before running each JavaScript test.
 * @type {Array<string>}
 */
var genIncludes = [];

/**
 * When true, add calls to set_preload_test_(fixture|name). This is needed when
 * |testType| === 'webui' || 'mojo_webui' to send an injection message before
 * the page loads, but is not required or supported by any other test type.
 * @type {boolean}
 */
var addSetPreloadInfo;

/**
 * Whether cc headers need to be generated.
 * @type {boolean}
 */
var needGenHeader = true;

/**
 * Helpful hint pointing back to the source js.
 * @type {string}
 */
var argHint = '// ' + this['arguments'].join(' ');

/**
 * @type {Array<string>}
 */
var pendingOutput = '';

/**
 * Adds a string followed by a newline to the pending output.
 * If present, an initial newline character is stripped from the string.
 * @param {string=} opt_string
 */
function output(opt_string) {
  opt_string = opt_string || '';
  if (opt_string[0] == '\n')
    opt_string = opt_string.substring(1);
    pendingOutput += opt_string;
  pendingOutput += '\n';
}

/**
 * Returns number of lines in pending output.
 * @returns {number}
 */
function countOutputLines() {
  return pendingOutput.split('\n').length;
}

/**
 * Generates the header of the cc file to stdout.
 * @param {string?} testFixture Name of test fixture.
 */
function maybeGenHeader(testFixture) {
  if (!needGenHeader)
    return;
  needGenHeader = false;
  output(`
// GENERATED FILE'
${argHint}
// PLEASE DO NOT HAND EDIT!
`);

  // Output some C++ headers based upon the |testType|.
  //
  // Currently supports:
  // 'extension' - browser_tests harness, js2extension rule,
  //               ExtensionJSBrowserTest superclass.
  // 'unit' - unit_tests harness, js2unit rule, V8UnitTest superclass.
  // 'mojo_webui' - browser_tests harness, js2webui rule, MojoWebUIBrowserTest
  // superclass. Uses Mojo to communicate test results.
  // 'webui' - browser_tests harness, js2webui rule, WebUIBrowserTest
  // superclass. Uses chrome.send to communicate test results.
  if (testType === 'extension') {
    output('#include "chrome/test/base/extension_js_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'ExtensionJSBrowserTest';
    addSetPreloadInfo = false;
    testF = 'IN_PROC_BROWSER_TEST_F';
  } else if (testType === 'unit') {
    output('#include "chrome/test/base/v8_unit_test.h"');
    testing.Test.prototype.typedefCppFixture = 'V8UnitTest';
    testF = 'TEST_F';
    addSetPreloadInfo = false;
  } else if (testType === 'mojo_webui') {
    output('#include "chrome/test/base/mojo_web_ui_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'MojoWebUIBrowserTest';
    testF = 'IN_PROC_BROWSER_TEST_F';
    addSetPreloadInfo = true;
  } else if (testType === 'webui') {
    output('#include "chrome/test/base/web_ui_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'WebUIBrowserTest';
    testF = 'IN_PROC_BROWSER_TEST_F';
    addSetPreloadInfo = true;
  }
  output(`
#include "url/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"`);
  // Add includes specified by test fixture.
  if (testFixture) {
    if (this[testFixture].prototype.testGenCppIncludes)
      this[testFixture].prototype.testGenCppIncludes();
    if (this[testFixture].prototype.commandLineSwitches)
      output('#include "base/command_line.h"');
    if (this[testFixture].prototype.featureList ||
        this[testFixture].prototype.featureWithParameters)
      output('#include "base/test/scoped_feature_list.h"');
  }
  output();
}


/**
 * @type {Array<{path: string, base: string>}
 */
var pathStack = [];


/**
 * Convert the |includeFile| to paths appropriate for immediate
 * inclusion (path) and runtime inclusion (base).
 * @param {string} includeFile The file to include.
 * @return {{path: string, base: string}} Object describing the paths
 *     for |includeFile|. |path| is relative to cwd; |base| is relative to
 *     source root.
 */
function includeFileToPaths(includeFile) {
  paths = pathStack[pathStack.length - 1];
  return {
    path: paths.path.replace(/[^\/\\]+$/, includeFile),
    base: paths.base.replace(/[^\/\\]+$/, includeFile),
  };
}

/**
 * Returns the content of a javascript file with a sourceURL comment
 * appended to facilitate better stack traces.
 * @param {string} path Relative path name.
 * return {string}
 */
function readJsFile(path) {
  return read(path) + '\n//# sourceURL=' + path;
}


/**
 * Maps object names to the path to the file that provides them.
 * Populated from the |depsFile| if any.
 * @type {Object<string>}
 */
var dependencyProvidesToPaths = {};

/**
 * Maps dependency path names to object names required by the file.
 * Populated from the |depsFile| if any.
 * @type {Object<Array<string>>}
 */
var dependencyPathsToRequires = {};

if (depsFile) {
  var goog = goog || {};
  /**
   * Called by the javascript in the deps file to add modules and their
   * dependencies.
   * @param {string} path Relative path to the file.
   * @param Array<string> provides Objects provided by this file.
   * @param Array<string> requires Objects required by this file.
   */
  goog.addDependency = function(path, provides, requires) {
    provides.forEach(function(provide) {
      dependencyProvidesToPaths[provide] = path;
    });
    dependencyPathsToRequires[path] = requires;
  };

  // Read and eval the deps file.  It should only contain goog.addDependency
  // calls.
  eval(readJsFile(depsFile));
}

/**
 * Resolves a list of libraries to an ordered list of paths to load by the
 * generated C++.  The input should contain object names provided
 * by the deps file.  Dependencies will be resolved and included in the
 * correct order, meaning that the returned array may contain more entries
 * than the input.
 * @param {Array<string>} deps List of dependencies.
 * @return {Array<string>} List of paths to load.
 */
function resolveClosureModuleDeps(deps) {
  if (!depsFile && deps.length > 0) {
    print('Can\'t have closure dependencies without a deps file.');
    quit(-1);
  }
  var resultPaths = [];
  var addedPaths = {};

  function addPath(path) {
    addedPaths[path] = true;
    resultPaths.push(path);
  }

  function resolveAndAppend(path) {
    if (addedPaths[path]) {
      return;
    }
    // Set before recursing to catch cycles.
    addedPaths[path] = true;
    dependencyPathsToRequires[path].forEach(function(require) {
      var providingPath = dependencyProvidesToPaths[require];
      if (!providingPath) {
        print('Unknown object', require, 'required by', path);
        quit(-1);
      }
      resolveAndAppend(providingPath);
    });
    resultPaths.push(path);
  }

  // Always add closure library's base.js if provided by deps.
  var basePath = dependencyProvidesToPaths['goog'];
  if (basePath) {
    addPath(basePath);
  }

  deps.forEach(function(dep) {
    var providingPath = dependencyProvidesToPaths[dep];
    if (providingPath) {
      resolveAndAppend(providingPath);
    } else {
      print('Unknown dependency:', dep);
      quit(-1);
    }
  });

  return resultPaths;
}

/**
 * Output |code| verbatim.
 * @param {string} code The code to output.
 */
function GEN(code) {
  maybeGenHeader(null);
  output(code);
}

/**
 * Outputs |commentEncodedCode|, converting comment to enclosed C++ code.
 * @param {function} commentEncodedCode A function in the following format (note
 * the space in '/ *' and '* /' should be removed to form a comment delimiter):
 *    function() {/ *! my_cpp_code.DoSomething(); * /
 *    Code between / *! and * / will be extracted and written to stdout.
 */
function GEN_BLOCK(commentEncodedCode) {
  var code = commentEncodedCode.toString().
      replace(/^[^\/]+\/\*!?/, '').
      replace(/\*\/[^\/]+$/, '').
      replace(/^\n|\n$/, '').
      replace(/\s+$/, '');
  GEN(code);
}

/**
 * Generate includes for the current |jsFile| by including them
 * immediately and at runtime.
 * The paths must be relative to the directory of the current file.
 * @param {Array<string>} includes Paths to JavaScript files to
 *     include immediately and at runtime.
 */
function GEN_INCLUDE(includes) {
  for (var i = 0; i < includes.length; i++) {
    var includePaths = includeFileToPaths(includes[i]);
    var js = readJsFile(includePaths.path);
    pathStack.push(includePaths);
    ('global', eval)(js);
    pathStack.pop();
    genIncludes.push(includePaths.base);
  }
}

/**
 * Capture stack-trace and find line number of TEST_F function call.
 * @return {Number} line number of TEST_F function call.
 */
function getTestDeclarationLineNumber() {
  var oldPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, structuredStackTrace) {
    return structuredStackTrace;
  };
  var error = Error('');
  Error.captureStackTrace(error, TEST_F);
  var lineNumber = error.stack[0].getLineNumber();
  Error.prepareStackTrace = oldPrepareStackTrace;
  return lineNumber;
}

/**
 * Generate gtest-style TEST_F definitions for C++ with a body that
 * will invoke the |testBody| for |testFixture|.|testFunction|.
 * @param {string} testFixture The name of this test's fixture.
 * @param {string} testFunction The name of this test's function.
 * @param {Function} testBody The function body to execute for this test.
 */
function TEST_F(testFixture, testFunction, testBody) {
  maybeGenHeader(testFixture);
  var browsePreload = this[testFixture].prototype.browsePreload;
  var browsePrintPreload = this[testFixture].prototype.browsePrintPreload;
  var testGenPreamble = this[testFixture].prototype.testGenPreamble;
  var testGenPostamble = this[testFixture].prototype.testGenPostamble;
  var typedefCppFixture = this[testFixture].prototype.typedefCppFixture;
  var isAsyncParam = testType === 'unit' ? '' :
      this[testFixture].prototype.isAsync + ',\n          ';
  var testShouldFail = this[testFixture].prototype.testShouldFail;
  var testPredicate = testShouldFail ? 'ASSERT_FALSE' : 'ASSERT_TRUE';
  var extraLibraries = genIncludes.concat(
      this[testFixture].prototype.extraLibraries.map(
          function(includeFile) {
            return includeFileToPaths(includeFile).base;
          }),
      resolveClosureModuleDeps(this[testFixture].prototype.closureModuleDeps));
  var testFLine = getTestDeclarationLineNumber();

  if (typedefCppFixture && !(testFixture in typedeffedCppFixtures)) {
    var switches = this[testFixture].prototype.commandLineSwitches;
    var hasSwitches = switches && switches.length;
    var featureList = this[testFixture].prototype.featureList;
    var featureWithParameters =
        this[testFixture].prototype.featureWithParameters;
    if ((!hasSwitches && !featureList && !featureWithParameters) ||
        typedefCppFixture == 'V8UnitTest') {
      output(`
typedef ${typedefCppFixture} ${testFixture};
`);
    } else {
      // Make the testFixture a class inheriting from the base fixture.
      output(`
class ${testFixture} : public ${typedefCppFixture} {
 protected:`);
      if (featureList || featureWithParameters) {
        output(`
  ${testFixture}() {`);
        if (featureList) {
          output(`
    scoped_feature_list_.InitWithFeatures({${featureList[0]}},
                                          {${featureList[1]}});`);
        }
        if (featureWithParameters) {
          var feature = featureWithParameters[0];
          var parameters = featureWithParameters[1];
          output(`
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ${feature}, {`);
          for (var parameter of parameters) {
            var parameterName = parameter[0];
            var parameterValue = parameter[1];
            output(`
            {"${parameterName}", "${parameterValue}"},`);
          }
          output(`
    });`);
        }
        output(`
  }`);
      } else {
        output(`
  ${testFixture}() {}`);
      }
      output(`
  ~${testFixture}() override {}
 private:`);
      if (hasSwitches) {
      // Override SetUpCommandLine and add each switch.
      output(`
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ${typedefCppFixture}::SetUpCommandLine(command_line);`);
      for (var i = 0; i < switches.length; i++) {
        output(`
    command_line->AppendSwitchASCII(
        "${switches[i].switchName}",
        "${(switches[i].switchValue || '')}");`);
      }
      output(`
  }`);
      }
      if (featureList || featureWithParameters) {
        output(`
  base::test::ScopedFeatureList scoped_feature_list_;`);
      }
      output(`
};
`);
    }
    typedeffedCppFixtures[testFixture] = typedefCppFixture;
  }

  var outputLine = countOutputLines() + 3;
  output(`
#line ${testFLine} "${jsFile}"
${testF}(${testFixture}, ${testFunction}) {
#line ${outputLine} "${outputFile}"`);

  for (var i = 0; i < extraLibraries.length; i++) {
    var libraryName = extraLibraries[i].replace(/\\/g, '/');
    output(`
  AddLibrary(base::FilePath(FILE_PATH_LITERAL(
      "${libraryName}")));`);
  }
  output(`
  AddLibrary(base::FilePath(FILE_PATH_LITERAL(
      "${jsFileBase.replace(/\\/g, '/')}")));`);
  if (addSetPreloadInfo) {
    output(`
  set_preload_test_fixture("${testFixture}");
  set_preload_test_name("${testFunction}");`);
  }
  if (testGenPreamble)
    testGenPreamble(testFixture, testFunction);
  if (browsePreload)
    output(`  BrowsePreload(GURL("${browsePreload}"));`);
  if (browsePrintPreload) {
    output(`
  BrowsePrintPreload(GURL(WebUITestDataPathToURL(
    FILE_PATH_LITERAL("${browsePrintPreload}"))));`);
  }
  output(`
  ${testPredicate}(
      RunJavascriptTestF(
          ${isAsyncParam}"${testFixture}",
          "${testFunction}"));`);
  if (testGenPostamble)
    testGenPostamble(testFixture, testFunction);
  output('}\n');
}

// Now that generation functions are defined, load in |jsFile|.
var js = readJsFile(jsFile);
pathStack.push({path: jsFile, base: jsFileBase});
eval(js);
pathStack.pop();
print(pendingOutput);
