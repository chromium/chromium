rem Deletes the script's grandparent directory if
rem \AppData\Local\@COMPANY_SHORTNAME@\@PRODUCT_FULLNAME@\ is anywhere in the
rem directory path.  Sleeps 3 seconds and tries 3 times to delete the
rem directory.
@echo off
set Directory=%~dp0
FOR %%a IN ("%Directory:~0,-1%") DO set Directory=%%~dpa
@echo %Directory% | FindStr /R \\AppData\\Local\\@COMPANY_SHORTNAME@\\@PRODUCT_FULLNAME@\\ > nul
IF %ERRORLEVEL% NEQ 0 exit 1
@echo Deleting "%Directory%"...
for /L %%G IN (1,1,3) do (
  rmdir "%Directory%" /s /q > nul
  if not exist "%Directory%" exit 0
  ping -n 3 127.0.0.1 > nul
)
