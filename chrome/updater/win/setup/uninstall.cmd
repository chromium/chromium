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

rem Try deleting the directory 15 times and wait one second between tries.
for /L %%G IN (1,1,15) do (
  ping -n 2 127.0.0.1 > nul
  rmdir %Directory% /s /q > nul
  if not exist %Directory% exit 0
)

exit 3

