// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generator script for creating gtest-style JavaScript
 *     tests for extensions, WebUI and unit tests. Generates C++ gtest wrappers
 *     which will invoke the appropriate JavaScript for each test.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see WebUI testing: http://goo.gl/ZWFXF
 * @see gtest documentation: http://goo.gl/Ujj3H
 * @see chrome/test/base/ash/js2gtest.gni
 * @see chrome/test/base/ash/js2gtest.js
 * @see chrome/test/base/ash/v8sh.py
 */

// Arguments from the js2gtest template in chrome/test/base/js2gtest.gni
// are passed in through python script v8sh.py.
if (arguments.length != 7) {
  print(
      'usage: ' + arguments[0] +
      ' path-to-testfile.js path-to-src-root/ path-to-deps.js output.cc' +
      ' test-type parameterized');
  quit(-1);
}

const [_,
 // Full path to the test input file, relative to the current working
 // directory.
 fullTestFilePath,
 // Path to source-root, relative to the current working directory.
 srcRootPath,
 // Path to Closure library style deps.js file.
 depsFile,
 // Path to C++ file generation is outputting to.
 outputFile,
 // Type of this test. One of 'extension', 'unit', 'webui', 'mojo_lite_webui',
 // 'mojo_webui'.
 testType,
 // Whether the test is parameterized. Either "true" or "false".
 parameterizedStr,
] = arguments;

const parameterized = parameterizedStr === 'true';

if (!fullTestFilePath.startsWith(srcRootPath)) {
  print('Test file must be a descendant of source root directory');
  quit(-1);
}

/**
 * Path to test input file, relative to source root directory.
 * @type {string}
 */
const testFile = fullTestFilePath.substr(srcRootPath.length);

const TEST_TYPES =
    new Set(['extension', 'unit', 'webui', 'mojo_lite_webui', 'mojo_webui']);

if (!TEST_TYPES.has(testType)) {
  print('Invalid test type: ' + testType);
  quit(-1);
}

/**
 * C++ gtest macro to use for TEST_F depending on |testType|.
 * @type {string} ('TEST_F'|'IN_PROC_BROWSER_TEST_F')
 */
let testF;

/**
 * Keeps track of whether a typedef has been generated for each test
 * fixture.
 * @type {!Map<string, string>}
 */
const typedeffedCppFixtures = new Map();

/**
 * Maintains a list of file paths (relative to source-root directory) to add
 * to each gtest body for inclusion at runtime before running each JS test.
 * @type {Array<string>}
 */
const genIncludes = [];

/**
 * When true, add calls to set_preload_test_(fixture|name). This is needed when
 * |testType| === 'webui' to send an injection message before the page loads,
 * but is not required or supported by any other test type.
 * @type {boolean}
 */
let addSetPreloadInfo;

/**
 * Whether cc headers need to be generated.
 * @type {boolean}
 */
let needGenHeader = true;

/**
 * Helpful hint pointing back to the source js.
 * @type {string}
 */
const argHint = '// ' + Array.from(arguments).join(' ');

/**
 * @type {Array<string>}
 */
const pendingOutput = [];

/**
 * Adds a string followed by a newline to the pending output.
 * If present, an initial newline character is stripped from the string.
 * @param {string=} opt_string
 */
function output(opt_string) {
  opt_string = opt_string || '';
  if (opt_string[0] == '\n') {
    opt_string = opt_string.substring(1);
  }
  pendingOutput.push(opt_string);
}

/**
 * Generates the header of the cc file to stdout.
 * @param {string?} testFixture Name of test fixture.
 * @this {!Object}
 */
function maybeGenHeader(testFixture) {
  if (!needGenHeader) {
    return;
  }
  needGenHeader = false;
  output(`
// GENERATED FILE
${argHint}
// PLEASE DO NOT HAND EDIT!
`);

  // Output some C++ headers based upon the |testType|.
  //
  // Currently supports:
  // 'extension' - browser_tests harness, js2extension rule,
  //               ExtensionJSBrowserTest superclass.
  // 'unit' - unit_tests harness, js2unit rule, V8UnitTest superclass.
  // 'mojo_lite_webui' - browser_tests harness, js2webui rule,
  //                     MojoWebUIBrowserTest with mojo_lite bindings. Uses Mojo
  //                     to communicate test results.
  // 'mojo_webui' - browser_tests harness, js2webui rule, MojoWebUIBrowserTest
  //                with mojo bindings JS modules. Uses Mojo to communicate test
  //                results.
  // 'webui' - browser_tests harness, js2webui rule, WebUIBrowserTest
  // superclass. Uses chrome.send to communicate test results.
  if (testType === 'extension') {
    output('#include "chrome/test/base/ash/extension_js_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'ExtensionJSBrowserTest';
    addSetPreloadInfo = false;
    if (parameterized) {
      testF = 'IN_PROC_BROWSER_TEST_P'
    } else {
      testF = 'IN_PROC_BROWSER_TEST_F';
    }
  } else if (testType === 'unit') {
    output('#include "chrome/test/base/ash/v8_unit_test.h"');
    testing.Test.prototype.typedefCppFixture = 'V8UnitTest';
    testF = 'TEST_F';
    addSetPreloadInfo = false;
  } else if (testType === 'mojo_lite_webui' || testType == 'mojo_webui') {
    output('#include "chrome/test/base/ash/mojo_web_ui_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'MojoWebUIBrowserTest';
    testF = 'IN_PROC_BROWSER_TEST_F';
    addSetPreloadInfo = true;
  } else if (testType === 'webui') {
    output('#include "chrome/test/base/ash/web_ui_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'WebUIBrowserTest';
    testF = 'IN_PROC_BROWSER_TEST_F';
    addSetPreloadInfo = true;
  }
  output(`
#include <tuple>

#include "url/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"`);
  // Add includes specified by test fixture.
  if (testFixture) {
    if (this[testFixture].prototype.testGenCppIncludes) {
      this[testFixture].prototype.testGenCppIncludes();
    }
    if (this[testFixture].prototype.commandLineSwitches) {
      output('#include "base/command_line.h"');
    }
    if (this[testFixture].prototype.featureList ||
        this[testFixture].prototype.featuresWithParameters ||
        this[testFixture].prototype.paramFeature) {
      output('#include "base/test/scoped_feature_list.h"');
    }
    if (this[testFixture].prototype.paramCommandLineSwitch) {
      output('#include "base/test/scoped_command_line.h"');
    }
  }
  output();
}


/** @type {!Array<string>} */
const pathStack = [];


/**
 * Get the path (relative to source root directory) of the given include-file.
 * The include must either be relative to the file it is included from,
 * or a source-absolute path starting with '//'.
 *
 * @param {string} includeFile The file to include.
 * @return {string} Path of the file, relative to source root directory.
 */
function includeFileToPath(includeFile) {
  if (includeFile.startsWith('//')) {
    return includeFile.substr(2);  // Path is already relative to source-root.
  } else if (includeFile.startsWith('/')) {
    print('Error including ' + includeFile);
    print(
        'Only relative "foo/bar" or source-absolute "//foo/bar" paths are ' +
        'supported - not file-system absolute: "/foo/bar"');
    quit(-1);
    return '';
  } else {
    // The include-file path is relative to the file that included it.
    const currentPath = pathStack[pathStack.length - 1];
    return currentPath.replace(/[^\/\\]+$/, includeFile);
  }
}

/**
 * Returns the content of a javascript file with a sourceURL comment
 * appended to facilitate better stack traces.
 * @param {string} path Path to JS file, relative to current working dir.
 * return {string}
 */
function readJsFile(path) {
  return read(path) + '\n//# sourceURL=' + path;
}

/**
 * Returns the content of a javascript file with a sourceURL comment
 * appended to facilitate better stack traces.
 * @param {string} path Path to JS file, relative to source root.
 * return {string}
 */
function readSourceAbsoluteJsFile(path) {
  return readJsFile(srcRootPath + path);
}


/**
 * Maps object names to the path to the file that provides them.
 * Populated from the |depsFile| if any.
 * @type {Object<string>}
 */
const dependencyProvidesToPaths = {};

/**
 * Maps dependency path names to object names required by the file.
 * Populated from the |depsFile| if any.
 * @type {Object<Array<string>>}
 */
const dependencyPathsToRequires = {};

if (depsFile) {
  var goog = goog || {};
  /**
   * Called by the javascript in the deps file to add modules and their
   * dependencies.
   * @param {string} path Relative path to the file.
   * @param {!Array<string>} provides Objects provided by this file.
   * @param {!Array<string>} requires Objects required by this file.
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
  const resultPaths = [];
  const addedPaths = {};

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
      const providingPath = dependencyProvidesToPaths[require];
      if (!providingPath) {
        print('Unknown object', require, 'required by', path);
        quit(-1);
      }
      resolveAndAppend(providingPath);
    });
    resultPaths.push(path);
  }

  // Always add closure library's base.js if provided by deps.
  const basePath = dependencyProvidesToPaths['goog'];
  if (basePath) {
    addPath(basePath);
  }

  deps.forEach(function(dep) {
    const providingPath = dependencyProvidesToPaths[dep];
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
 * Generate includes for the current |jsFile| by including them
 * immediately and at runtime.
 * The paths must be relative to the directory of the current file.
 * @param {Array<string>} includes Paths to JavaScript files to
 *     include immediately and at runtime.
 */
function GEN_INCLUDE(includes) {
  for (let i = 0; i < includes.length; i++) {
    const includePath = includeFileToPath(includes[i]);
    const js = readSourceAbsoluteJsFile(includePath);
    pathStack.push(includePath);
    ('global', eval)(js);
    pathStack.pop();
    genIncludes.push(includePath);
  }
}

/**
 * Capture stack-trace and find line number of TEST_F function call.
 * @return {Number} line number of TEST_F function call.
 */
function getTestDeclarationLineNumber() {
  const oldPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, structuredStackTrace) {
    return structuredStackTrace;
  };
  const error = Error('');
  Error.captureStackTrace(error, TEST_F);
  const lineNumber = error.stack[0].getLineNumber();
  Error.prepareStackTrace = oldPrepareStackTrace;
  return lineNumber;
}

/**
 * Generate gtest-style TEST_F definitions for C++ with a body that
 * will invoke the |testBody| for |testFixture|.|testFunction|.
 * @param {string} testFixture The name of this test's fixture.
 * @param {string} testFunction The name of this test's function.
 * @param {!Function} testBody The function body to execute for this test.
 * @param {string=} opt_preamble C++ to be generated before the TEST_F block.
 * Useful for including #ifdef blocks. See TEST_F_WITH_PREAMBLE.
 * @this {!Object}
 */
function TEST_F(testFixture, testFunction, testBody, opt_preamble) {
  maybeGenHeader(testFixture);
  const browsePreload = this[testFixture].prototype.browsePreload;
  const testGenPreamble = this[testFixture].prototype.testGenPreamble;
  const testGenPostamble = this[testFixture].prototype.testGenPostamble;
  const typedefCppFixture = this[testFixture].prototype.typedefCppFixture;
  const isAsyncParam = testType === 'unit' ?
      '' :
      this[testFixture].prototype.isAsync + ',\n          ';
  const testShouldFail = this[testFixture].prototype.testShouldFail;
  const testPredicate = testShouldFail ? 'ASSERT_FALSE' : 'ASSERT_TRUE';
  const webuiHost = this[testFixture].prototype.webuiHost;
  const extraLibraries = genIncludes.concat(
      this[testFixture].prototype.extraLibraries.map(includeFileToPath),
      resolveClosureModuleDeps(this[testFixture].prototype.closureModuleDeps),
      [testFile]);
  const testFLine = getTestDeclarationLineNumber();
  const testServer = this[testFixture].prototype.testServer;

  if (typedefCppFixture && !typedeffedCppFixtures.has(testFixture)) {
    const switches = this[testFixture].prototype.commandLineSwitches;
    const hasSwitches = switches && switches.length;
    const featureList = this[testFixture].prototype.featureList;
    const featuresWithParameters =
        this[testFixture].prototype.featuresWithParameters;
    if ((!hasSwitches && !featureList && !featuresWithParameters &&
         !testServer && !parameterized) ||
        typedefCppFixture == 'V8UnitTest') {
      output(`
typedef ${typedefCppFixture} ${testFixture};
`);
    } else {
      let ancestors = `public ${typedefCppFixture}`;
      if (parameterized) {
        ancestors += `, public testing::WithParamInterface<bool>`;
      }

      // Make the testFixture a class inheriting from the base fixture.
      output(`
class ${testFixture} : ${ancestors} {
 protected:`);
      if (featureList || featuresWithParameters || testServer) {
        output(`
  ${testFixture}() {`);
        if (featureList) {
          const disabledFeatures = (featureList.disabled || []).join(', ');
          const enabledFeatures = (featureList.enabled || []).join(', ');
          if (enabledFeatures.length + disabledFeatures.length == 0) {
            print('Invalid featureList; must set "enabled" or "disabled" key');
            quit(-1);
          }
          output(`
    scoped_feature_list_.InitWithFeatures({${enabledFeatures}},
                                          {${disabledFeatures}});`);
        }
        if (featuresWithParameters) {
          for (let i = 0; i < featuresWithParameters.length; ++i) {
            const feature = featuresWithParameters[i];
            const featureName = feature.featureName;
            if (!featureName) {
              print('"featureName" key required for featuresWithParameters');
              quit(-1);
            }
            const parameters = feature.parameters;
            if (!parameters) {
              print('"parameters" key required for featuresWithParameters');
              quit(-1);
            }
            output(`
    scoped_feature_list${i}_.InitAndEnableFeatureWithParameters(
        ${featureName}, {`);
            for (const parameter of parameters) {
              const parameterName = parameter.name;
              if (!parameterName) {
                print(
                    '"name" key required for parameter in ' +
                    'featuresWithParameters');
                quit(-1);
              }
              const parameterValue = parameter.value;
              if (!parameterValue) {
                print(
                    '"value" key required for parameter in ' +
                    'featuresWithParameters');
                quit(-1);
              }
              output(`
            {"${parameterName}", "${parameterValue}"},`);
            }
            output(`
    });`);
          }
        }
        if (testServer) {
          output(`
    std::ignore = embedded_test_server()->Start();`);
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
      if (parameterized) {
        output(`
  void SetUp() override {
    `);
        if (this[testFixture].prototype.paramCommandLineSwitch) {
          const switchName = this[testFixture].prototype.paramCommandLineSwitch;
          output(`
    if (GetParam()) {
      scoped_command_line_.GetProcessCommandLine()->AppendSwitch(${switchName});
    } else {
      scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(${switchName});
    }
    ${typedefCppFixture}::SetUp();
  }

  base::test::ScopedCommandLine scoped_command_line_;`);
        } else if (this[testFixture].prototype.paramFeature) {
          const feature = this[testFixture].prototye.paramFeature;
          output(`
    scoped_feature_list_.InitWithFeatureState(${feature}, GetParam());
    ${typedefCppFixture}::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
        `)
        } else {
          output(`
    ${typedefCppFixture}::SetUp();
  }`);
        }
      }
      if (hasSwitches) {
        // Override SetUpCommandLine and add each switch.
        output(`
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ${typedefCppFixture}::SetUpCommandLine(command_line);`);
        for (let i = 0; i < switches.length; i++) {
          output(`
    command_line->AppendSwitchASCII(
        "${switches[i].switchName}",
        "${(switches[i].switchValue || '')}");`);
        }
        output(`
  }`);
      }
      if (featureList || featuresWithParameters) {
        if (featureList && !this[testFixture].prototype.paramFeature) {
          output(`
  base::test::ScopedFeatureList scoped_feature_list_;`);
        }
        if (featuresWithParameters) {
          for (let i = 0; i < featuresWithParameters.length; ++i) {
            output(`
  base::test::ScopedFeatureList scoped_feature_list${i}_;`);
          }
        }
      }
      output(`
};
`);
      if (parameterized) {
        output(`
INSTANTIATE_TEST_SUITE_P(All, ${testFixture}, testing::Bool());`);
      }
    }
    typedeffedCppFixtures.set(testFixture, typedefCppFixture);
  }

  if (opt_preamble) {
    GEN(opt_preamble);
  }

  const outputLine = pendingOutput.length + 3;
  output(`
#line ${testFLine} "${fullTestFilePath}"
${testF}(${testFixture}, ${testFunction}) {
#line ${outputLine} "${outputFile}"`);

  for (let i = 0; i < extraLibraries.length; i++) {
    const libraryName = extraLibraries[i].replace(/\\/g, '/');
    output(`
  AddLibrary(base::FilePath(FILE_PATH_LITERAL(
      "${libraryName}")));`);
  }
  if (addSetPreloadInfo) {
    output(`
  set_preload_test_fixture("${testFixture}");
  set_preload_test_name("${testFunction}");`);
  }
  if (testType == 'mojo_webui') {
    output(`
  set_use_mojo_modules();`);
  }
  if (webuiHost) {
    output(`
  set_webui_host("${webuiHost}");`);
  }
  if (testGenPreamble) {
    testGenPreamble(testFixture, testFunction);
  }
  if (browsePreload) {
    output(`  BrowsePreload(GURL("${browsePreload}"));`);
  }
  output(`
  ${testPredicate}(
      RunJavascriptTestF(
          ${isAsyncParam}"${testFixture}",
          "${testFunction}"));`);
  if (testGenPostamble) {
    testGenPostamble(testFixture, testFunction);
  }
  output('}\n');
}

/**
 * Same as TEST_F above, with a mandatory preamble.
 * @param {string} preamble C++ to be generated before the TEST_F block.
 *                 Useful for including #ifdef blocks.
 * @param {string} testFixture The name of this test's fixture.
 * @param {string} testFunction The name of this test's function.
 * @param {!Function} testBody The function body to execute for this test.
 */
function TEST_F_WITH_PREAMBLE(preamble, testFixture, testFunction, testBody) {
  TEST_F(testFixture, testFunction, testBody, preamble);
}

// Now that generation functions are defined, load in |testFile|.
const js = readSourceAbsoluteJsFile(testFile);
pathStack.push(testFile);
eval(js);
pathStack.pop();
print(pendingOutput.join('\n'));
