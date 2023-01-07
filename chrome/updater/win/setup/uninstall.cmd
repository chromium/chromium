@echo off

rem Deletes recursively the directory specified by the `--dir` command line
rem argument of the script. The directory must be an updater install path.

echo %1 %2
if not "%1"=="--dir" (
  echo "Invalid switch."
  exit 1
)

set Directory=%2

rem Validate the path is an updater path.
@echo %Directory% | FindStr /L \@COMPANY_SHORTNAME@\@PRODUCT_FULLNAME@ > nul
if %ERRORLEVEL% NEQ 0 (
  echo "Invalid argument."
  exit 2
)

for /L %%G IN (1,1,3) do (
  rmdir %Directory% /s /q > nul
  if not exist %Directory% exit 0
  ping -n 3 127.0.0.1 > nul
)

exit 3

